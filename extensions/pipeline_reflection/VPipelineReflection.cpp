// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VPipelineReflection.h"
#include "framegraph/Shared/EnumUtils.h"
#include "stl/Algorithms/StringUtils.h"

#include "spirv_reflect.h"

namespace FG
{
	
	static bool GetReflection (const Array<uint> &, bool, INOUT GraphicsPipelineDesc::FragmentOutputs_t &,
								INOUT GraphicsPipelineDesc::VertexAttribs_t &, INOUT PipelineDescription::PipelineLayout &);

	static bool GetReflection (const Array<uint> &, bool, INOUT uint3 &, INOUT uint3 &, INOUT PipelineDescription::PipelineLayout &);
	
	using ShaderDataUnion_t	= PipelineDescription::ShaderDataUnion_t;
	using SpirvShaderData_t	= PipelineDescription::SpirvShaderPtr;

/*
=================================================
	Reflect
=================================================
*/
	bool  VPipelineReflection::Reflect (INOUT MeshPipelineDesc &ppln)
	{
		// TODO
		Unused( ppln );
		return false;
	}
	
/*
=================================================
	Reflect
=================================================
*/
	bool  VPipelineReflection::Reflect (INOUT RayTracingPipelineDesc &ppln)
	{
		// TODO
		Unused( ppln );
		return false;
	}
	
/*
=================================================
	Reflect
=================================================
*/
	bool  VPipelineReflection::Reflect (INOUT GraphicsPipelineDesc &ppln)
	{
		ShaderDataUnion_t const*	any_sh_data  = null;
		ShaderDataUnion_t const*	best_sh_data = null;
		
		GraphicsPipelineDesc::FragmentOutputs_t	fragment_output;
		GraphicsPipelineDesc::VertexAttribs_t	vertex_attribs;
		PipelineDescription::PipelineLayout		pipeline_layout;

		for (auto& sh : ppln._shaders)
		{
			any_sh_data = best_sh_data = null;

			for (auto& d : sh.second.data)
			{
				if ( not AllBits( d.first, EShaderLangFormat::SPIRV ))
					continue;

				if ( not AnyBits( d.first, EShaderLangFormat::EnableDebugTrace | EShaderLangFormat::EnableProfiling ))
				{
					best_sh_data = &d.second;
					break;
				}

				if ( not any_sh_data )
					any_sh_data = &d.second;
			}

			if ( not best_sh_data )
				best_sh_data = any_sh_data;

			if ( not best_sh_data )
				continue;	// TODO: error?

			auto* spirv = UnionGetIf<SpirvShaderData_t>( best_sh_data );
			CHECK_ERR( spirv and *spirv );

			CHECK_ERR( GetReflection( (*spirv)->GetData(), false, OUT fragment_output, OUT vertex_attribs, OUT pipeline_layout ));
		}
		
		ppln._fragmentOutput	= fragment_output;
		ppln._vertexAttribs		= vertex_attribs;
		ppln._pipelineLayout	= pipeline_layout;

		return true;
	}
	
/*
=================================================
	Reflect
=================================================
*/
	bool  VPipelineReflection::Reflect (INOUT ComputePipelineDesc &ppln)
	{
		ShaderDataUnion_t const*	any_sh_data  = null;
		ShaderDataUnion_t const*	best_sh_data = null;

		for (auto& d : ppln._shader.data)
		{
			if ( not AllBits( d.first, EShaderLangFormat::SPIRV ))
				continue;

			if ( not AnyBits( d.first, EShaderLangFormat::EnableDebugTrace | EShaderLangFormat::EnableProfiling ))
			{
				best_sh_data = &d.second;
				break;
			}

			if ( not any_sh_data )
				any_sh_data = &d.second;
		}

		if ( not best_sh_data )
			best_sh_data = any_sh_data;

		if ( not best_sh_data )
			return false;
		
		auto* spirv = UnionGetIf<SpirvShaderData_t>( best_sh_data );
		CHECK_ERR( spirv and *spirv );

		CHECK_ERR( GetReflection( (*spirv)->GetData(), false, OUT ppln._defaultLocalGroupSize, OUT ppln._localSizeSpec, OUT ppln._pipelineLayout ));
		return true;
	}
	
/*
=================================================
	MergeShaderAccess
=================================================
*/
	static void MergeShaderAccess (const EResourceState srcAccess, INOUT EResourceState &dstAccess)
	{
		if ( srcAccess == dstAccess )
			return;

		dstAccess |= srcAccess;

		if ( AllBits( dstAccess, EResourceState::InvalidateBefore ) and
			 AllBits( dstAccess, EResourceState::ShaderRead ))
		{
			dstAccess &= ~EResourceState::InvalidateBefore;
		}
	}

/*
=================================================
	MergeUniforms
=================================================
*/
	static bool MergeUniforms (const PipelineDescription::UniformMap_t &srcUniforms, INOUT PipelineDescription::UniformMap_t &dstUniforms)
	{
		for (auto& un : srcUniforms)
		{
			auto	iter = dstUniforms.find( un.first );

			// add new uniform
			if ( iter == dstUniforms.end() )
			{
				dstUniforms.insert( un );
				continue;
			}

			bool	type_mismatch = true;

			Visit( un.second.data,
				[&] (const PipelineDescription::Texture &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::Texture>( &iter->second.data ))
					{
						ASSERT( lhs.textureType	== rhs->textureType );
						ASSERT( un.second.index	== iter->second.index );

						if ( lhs.textureType	== rhs->textureType and
							 un.second.index	== iter->second.index )
						{
							iter->second.stageFlags |= un.second.stageFlags;
							rhs->state				|= EResourceState_FromShaders( iter->second.stageFlags );
							type_mismatch			 = false;
						}
					}
				},
				   
				[&] (const PipelineDescription::Sampler &)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::Sampler>( &iter->second.data ))
					{
						ASSERT( un.second.index == iter->second.index );

						if ( un.second.index == iter->second.index )
						{
							iter->second.stageFlags |= un.second.stageFlags;
							type_mismatch			 = false;
						}
					}
				},
				
				[&] (const PipelineDescription::SubpassInput &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::SubpassInput>( &iter->second.data ))
					{
						ASSERT( lhs.attachmentIndex	== rhs->attachmentIndex );
						ASSERT( lhs.isMultisample	== rhs->isMultisample );
						ASSERT( un.second.index		== iter->second.index );
						
						if ( lhs.attachmentIndex	== rhs->attachmentIndex	and
							 lhs.isMultisample		== rhs->isMultisample	and
							 un.second.index		== iter->second.index )
						{
							iter->second.stageFlags |= un.second.stageFlags;
							rhs->state				|= EResourceState_FromShaders( iter->second.stageFlags );
							type_mismatch			 = false;
						}
					}
				},
				
				[&] (const PipelineDescription::Image &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::Image>( &iter->second.data ))
					{
						ASSERT( lhs.imageType	== rhs->imageType );
						ASSERT( un.second.index	== iter->second.index );
						
						if ( lhs.imageType		== rhs->imageType	and
							 un.second.index	== iter->second.index )
						{
							MergeShaderAccess( lhs.state, INOUT rhs->state );

							iter->second.stageFlags |= un.second.stageFlags;
							rhs->state				|= EResourceState_FromShaders( iter->second.stageFlags );
							type_mismatch			 = false;
						}
					}
				},
				
				[&] (const PipelineDescription::UniformBuffer &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::UniformBuffer>( &iter->second.data ))
					{
						ASSERT( lhs.size		== rhs->size );
						ASSERT( un.second.index	== iter->second.index );

						if ( lhs.size			== rhs->size	and
							 un.second.index	== iter->second.index )
						{
							iter->second.stageFlags |= un.second.stageFlags;
							rhs->state				|= EResourceState_FromShaders( iter->second.stageFlags );
							type_mismatch			 = false;
						}
					}
				},
				
				[&] (const PipelineDescription::StorageBuffer &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::StorageBuffer>( &iter->second.data ))
					{
						ASSERT( lhs.staticSize	== rhs->staticSize );
						ASSERT( lhs.arrayStride	== rhs->arrayStride );
						ASSERT( un.second.index	== iter->second.index );
						
						if ( lhs.staticSize		== rhs->staticSize	and
							 lhs.arrayStride	== rhs->arrayStride	and
							 un.second.index	== iter->second.index )
						{
							MergeShaderAccess( lhs.state, INOUT rhs->state );

							iter->second.stageFlags |= un.second.stageFlags;
							rhs->state				|= EResourceState_FromShaders( iter->second.stageFlags );
							type_mismatch			 = false;
						}
					}
				},
					
				[&] (const PipelineDescription::RayTracingScene &lhs)
				{
					if ( auto* rhs = UnionGetIf<PipelineDescription::RayTracingScene>( &iter->second.data ))
					{
						ASSERT( lhs.state == rhs->state );

						if ( lhs.state == rhs->state )
						{
							iter->second.stageFlags |= un.second.stageFlags;
							type_mismatch			 = false;
						}
					}
				},

				[] (const NullUnion &) { ASSERT(false); }
			);

			CHECK_ERR( not type_mismatch );
		}
		return true;
	}

/*
=================================================
	FGEnumCast (SpvReflectShaderStageFlagBits)
=================================================
*/
	ND_ static EShaderStages  FGEnumCast (SpvReflectShaderStageFlagBits value)
	{
		BEGIN_ENUM_CHECKS();
		switch ( value )
		{
			case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT :					return EShaderStages::Vertex;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT :	return EShaderStages::TessControl;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT :	return EShaderStages::TessEvaluation;
			case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT :				return EShaderStages::Geometry;
			case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT :				return EShaderStages::Fragment;
			case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT :					return EShaderStages::Compute;
			case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV :					return EShaderStages::Mesh;
			case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV :					return EShaderStages::MeshTask;
			default: break;
		}
		END_ENUM_CHECKS();
		RETURN_ERR( "unknown shader stage!" );
	}

/*
=================================================
	FGEnumCast (SpvReflectImageTraits)
=================================================
*/
	ND_ static EImageSampler  FGEnumCast (const SpvReflectImageTraits &value)
	{
		BEGIN_ENUM_CHECKS();
		switch ( value.dim )
		{
			case SpvDim1D :		return value.arrayed ? EImageSampler::_1DArray : EImageSampler::_1D;
			case SpvDim2D :		return value.ms ?
										(value.arrayed ? EImageSampler::_2DMSArray : EImageSampler::_2DMS) :
										(value.arrayed ? EImageSampler::_2DArray : EImageSampler::_2D);
			case SpvDim3D :		return EImageSampler::_3D;
			case SpvDimCube :	return value.arrayed ? EImageSampler::_CubeArray : EImageSampler::_Cube;
			case SpvDimRect :
			case SpvDimBuffer :
			case SpvDimSubpassData :
			case SpvDimMax :	break;
		}
		END_ENUM_CHECKS();
		RETURN_ERR( "unsupported image dimension type" );
	}

/*
=================================================
	FGEnumCast (SpvReflectImageTraits)
=================================================
*/
	ND_ static EImageSampler  FGEnumCast (const SpvReflectImageTraits &value, SpvImageFormat fmt)
	{
		EImageSampler	result = Zero;

		BEGIN_ENUM_CHECKS();
		switch ( fmt )
		{
			case SpvImageFormatRgba32f :		result = EImageSampler(EPixelFormat::RGBA32F);			break;
			case SpvImageFormatRgba16f :		result = EImageSampler(EPixelFormat::RGBA16F);			break;
			case SpvImageFormatR32f :			result = EImageSampler(EPixelFormat::R32F);				break;
			case SpvImageFormatRgba8 :			result = EImageSampler(EPixelFormat::RGBA8_UNorm);		break;
			case SpvImageFormatRgba8Snorm :		result = EImageSampler(EPixelFormat::RGBA8_SNorm);		break;
			case SpvImageFormatRg32f :			result = EImageSampler(EPixelFormat::RG32F);			break;
			case SpvImageFormatRg16f :			result = EImageSampler(EPixelFormat::RG16F);			break;
			case SpvImageFormatR11fG11fB10f :	result = EImageSampler(EPixelFormat::RGB_11_11_10F);	break;
			case SpvImageFormatR16f :			result = EImageSampler(EPixelFormat::R16F);				break;
			case SpvImageFormatRgba16 :			result = EImageSampler(EPixelFormat::RGBA16_UNorm);		break;
			case SpvImageFormatRgb10A2 :		result = EImageSampler(EPixelFormat::RGB10_A2_UNorm);	break;
			case SpvImageFormatRg16 :			result = EImageSampler(EPixelFormat::RG16_UNorm);		break;
			case SpvImageFormatRg8 :			result = EImageSampler(EPixelFormat::RG8_UNorm);		break;
			case SpvImageFormatR16 :			result = EImageSampler(EPixelFormat::R16_UNorm);		break;
			case SpvImageFormatR8 :				result = EImageSampler(EPixelFormat::R8_UNorm);			break;
			case SpvImageFormatRgba16Snorm :	result = EImageSampler(EPixelFormat::RGBA16_SNorm);		break;
			case SpvImageFormatRg16Snorm :		result = EImageSampler(EPixelFormat::RG16_SNorm);		break;
			case SpvImageFormatRg8Snorm :		result = EImageSampler(EPixelFormat::RG8_SNorm);		break;
			case SpvImageFormatR16Snorm :		result = EImageSampler(EPixelFormat::R16_SNorm);		break;
			case SpvImageFormatR8Snorm :		result = EImageSampler(EPixelFormat::R8_SNorm);			break;
			case SpvImageFormatRgba32i :		result = EImageSampler(EPixelFormat::RGBA32I);			break;
			case SpvImageFormatRgba16i :		result = EImageSampler(EPixelFormat::RGBA16I);			break;
			case SpvImageFormatRgba8i :			result = EImageSampler(EPixelFormat::RGBA8I);			break;
			case SpvImageFormatR32i :			result = EImageSampler(EPixelFormat::R32I);				break;
			case SpvImageFormatRg32i :			result = EImageSampler(EPixelFormat::RG32I);			break;
			case SpvImageFormatRg16i :			result = EImageSampler(EPixelFormat::RG16I);			break;
			case SpvImageFormatRg8i :			result = EImageSampler(EPixelFormat::RG8I);				break;
			case SpvImageFormatR16i :			result = EImageSampler(EPixelFormat::R16I);				break;
			case SpvImageFormatR8i :			result = EImageSampler(EPixelFormat::R8I);				break;
			case SpvImageFormatRgba32ui :		result = EImageSampler(EPixelFormat::RGBA32U);			break;
			case SpvImageFormatRgba16ui :		result = EImageSampler(EPixelFormat::RGBA16U);			break;
			case SpvImageFormatRgba8ui :		result = EImageSampler(EPixelFormat::RGBA8U);			break;
			case SpvImageFormatR32ui :			result = EImageSampler(EPixelFormat::R32U);				break;
			case SpvImageFormatRgb10a2ui :		result = EImageSampler(EPixelFormat::RGB10_A2U);		break;
			case SpvImageFormatRg32ui :			result = EImageSampler(EPixelFormat::RG32U);			break;
			case SpvImageFormatRg16ui :			result = EImageSampler(EPixelFormat::RG16U);			break;
			case SpvImageFormatRg8ui :			result = EImageSampler(EPixelFormat::RG8U);				break;
			case SpvImageFormatR16ui :			result = EImageSampler(EPixelFormat::R16U);				break;
			case SpvImageFormatR8ui :			result = EImageSampler(EPixelFormat::R8U);				break;
			case SpvImageFormatUnknown :
			case SpvImageFormatMax :
			default :							RETURN_ERR( "unsupported pixel format!" );
		}
		switch ( value.dim )
		{
			case SpvDim1D :		result |= (value.arrayed ? EImageSampler::_1DArray : EImageSampler::_1D);		break;
			case SpvDim2D :		result |= (value.ms ?
											(value.arrayed ? EImageSampler::_2DMSArray : EImageSampler::_2DMS) :
											(value.arrayed ? EImageSampler::_2DArray : EImageSampler::_2D));	break;
			case SpvDim3D :		result |= EImageSampler::_3D;													break;
			case SpvDimCube :	result |= (value.arrayed ? EImageSampler::_CubeArray : EImageSampler::_Cube);	break;
			case SpvDimRect :
			case SpvDimBuffer :
			case SpvDimSubpassData :
			case SpvDimMax :
			default :			RETURN_ERR( "unsupported image dimension type" );
		}
		END_ENUM_CHECKS();
		return result;
	}

/*
=================================================
	GetDescriptorSetLayoutReflection
=================================================
*/
	static bool GetDescriptorSetLayoutReflection (const SpvReflectDescriptorSet &srcDS, EShaderStages shaderStage, bool forceDBO,
												  INOUT PipelineDescription::PipelineLayout &layout)
	{
		PipelineDescription::DescriptorSet	dst_ds;
		PipelineDescription::UniformMap_t	uniforms;
		EResourceState						rs_stages	= EResourceState_FromShaders( shaderStage );

		dst_ds.id = DescriptorSetID( "ds"s << ToString(srcDS.set) );
		dst_ds.bindingIndex = srcDS.set;

		for (uint i = 0; i < srcDS.binding_count; ++i)
		{
			auto&							src = *(srcDS.bindings[i]);
			PipelineDescription::Uniform	dst;
			UniformID						name;
			
			if ( src.type_description and src.type_description->type_name and
				 (src.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER or
				  src.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC or
				  src.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER or
				  src.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) )
				name = UniformID{ src.type_description->type_name };
			else
			if ( src.name and strlen(src.name) > 0 )
				name = UniformID{ src.name };
			else
				name = UniformID{ "un"s << ToString(src.binding) };


			dst.index		= BindingIndex{ UMax, src.binding };
			dst.stageFlags	= shaderStage;

			BEGIN_ENUM_CHECKS();
			switch ( src.descriptor_type )
			{
				case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER :
					dst.data = PipelineDescription::Sampler{};
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
					dst.data = PipelineDescription::Texture{ rs_stages | EResourceState::ShaderSample, FGEnumCast(src.image) };
					break;
					
				case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE :
					dst.data = PipelineDescription::Image{ rs_stages | EResourceState::ShaderSample, FGEnumCast(src.image) };
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE :
					dst.data = PipelineDescription::Image{ rs_stages | EResourceState::ShaderReadWrite, FGEnumCast(src.image, src.image.image_format) };
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT :
					dst.data = PipelineDescription::SubpassInput{ rs_stages | EResourceState::InputAttachment, src.input_attachment_index, !!src.image.ms };
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
					dst.data = PipelineDescription::UniformBuffer{ rs_stages | EResourceState::UniformRead | (forceDBO ? EResourceState::_BufferDynamicOffset : Default),
																	(forceDBO ? 0u : UMax), BytesU(src.block.size) };
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC :
					dst.data = PipelineDescription::UniformBuffer{ rs_stages | EResourceState::UniformRead | EResourceState::_BufferDynamicOffset,
																	0u, BytesU(src.block.size) };
					break;
					
				case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER :
					dst.data = PipelineDescription::StorageBuffer{ rs_stages | EResourceState::ShaderReadWrite | (forceDBO ? EResourceState::_BufferDynamicOffset : Default),
																	(forceDBO ? 0u : UMax), BytesU(src.block.size), 1_b };	// TODO: array stride
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC :
					dst.data = PipelineDescription::StorageBuffer{ rs_stages | EResourceState::ShaderReadWrite | EResourceState::_BufferDynamicOffset,
																	0u, BytesU(src.block.size), 1_b };
					break;

				case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER :
				case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
				default :
					RETURN_ERR( "unsupported descriptor type" );
			}
			END_ENUM_CHECKS();

			uniforms.insert_or_assign( name, std::move(dst) );
		}

		dst_ds.uniforms = MakeShared< PipelineDescription::UniformMap_t >( std::move(uniforms) );
		
		// merge descriptor sets
		for (auto& ds : layout.descriptorSets)
		{
			if ( ds.id == dst_ds.id )
			{
				CHECK_ERR( ds.bindingIndex == dst_ds.bindingIndex );
				CHECK_ERR( MergeUniforms( *dst_ds.uniforms, INOUT const_cast<PipelineDescription::UniformMap_t &>(*ds.uniforms) ));
				return true;
			}
		}

		// add new descriptor set
		layout.descriptorSets.push_back( dst_ds );
		return true;
	}
	
/*
=================================================
	GetPushConstantReflection
=================================================
*/
	static bool GetPushConstantReflection (const SpvReflectBlockVariable &srcPC, EShaderStages stageFlags,
										   INOUT PipelineDescription::PipelineLayout &layout)
	{
		PushConstantID	id		{ "pc0" };	//{ srcPC.name };
		BytesU			offset	{ srcPC.offset };
		BytesU			size	{ srcPC.padded_size };

		ASSERT( srcPC.size <= size );
		ASSERT( srcPC.absolute_offset == offset );	// TODO

		auto	iter = layout.pushConstants.find( id );

		// merge
		if ( iter != layout.pushConstants.end() )
		{
			CHECK_ERR( offset == uint(iter->second.offset) );
			CHECK_ERR( size == uint(iter->second.size) );

			iter->second.stageFlags |= stageFlags;
			return true;
		}

		// add new push constant
		layout.pushConstants.insert({ id, {stageFlags, offset, size} });
		return true;
	}

/*
=================================================
	FGEnumCastToVertexType
=================================================
*/
	ND_ static EVertexType  FGEnumCastToVertexType (SpvReflectFormat value)
	{
		BEGIN_ENUM_CHECKS();
		switch ( value )
		{
			case SPV_REFLECT_FORMAT_UNDEFINED :				break;
			case SPV_REFLECT_FORMAT_R32_UINT :				return EVertexType::UInt;
			case SPV_REFLECT_FORMAT_R32_SINT :				return EVertexType::Int;
			case SPV_REFLECT_FORMAT_R32_SFLOAT :			return EVertexType::Float;
			case SPV_REFLECT_FORMAT_R32G32_UINT :			return EVertexType::UInt2;
			case SPV_REFLECT_FORMAT_R32G32_SINT :			return EVertexType::Int2;
			case SPV_REFLECT_FORMAT_R32G32_SFLOAT :			return EVertexType::Float2;
			case SPV_REFLECT_FORMAT_R32G32B32_UINT :		return EVertexType::UInt3;
			case SPV_REFLECT_FORMAT_R32G32B32_SINT :		return EVertexType::Int3;
			case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT :		return EVertexType::Float3;
			case SPV_REFLECT_FORMAT_R32G32B32A32_UINT :		return EVertexType::UInt4;
			case SPV_REFLECT_FORMAT_R32G32B32A32_SINT :		return EVertexType::Int4;
			case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT :	return EVertexType::Float4;
		}
		END_ENUM_CHECKS();
		RETURN_ERR( "unknown vertex fromat!" );
	}

/*
=================================================
	FGEnumCastToFragOutput
=================================================
*/
	ND_ static EFragOutput  FGEnumCastToFragOutput (SpvReflectFormat value)
	{
		BEGIN_ENUM_CHECKS();
		switch ( value )
		{
			case SPV_REFLECT_FORMAT_UNDEFINED :				break;
			case SPV_REFLECT_FORMAT_R32_UINT :
			case SPV_REFLECT_FORMAT_R32G32_UINT :
			case SPV_REFLECT_FORMAT_R32G32B32_UINT :
			case SPV_REFLECT_FORMAT_R32G32B32A32_UINT :		return EFragOutput::UInt4;
			case SPV_REFLECT_FORMAT_R32_SINT :
			case SPV_REFLECT_FORMAT_R32G32_SINT :
			case SPV_REFLECT_FORMAT_R32G32B32_SINT :
			case SPV_REFLECT_FORMAT_R32G32B32A32_SINT :		return EFragOutput::Int4;
			case SPV_REFLECT_FORMAT_R32_SFLOAT :
			case SPV_REFLECT_FORMAT_R32G32_SFLOAT :
			case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT :
			case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT :	return EFragOutput::Float4;
		}
		END_ENUM_CHECKS();
		RETURN_ERR( "unknown vertex fromat!" );
	}
	
/*
=================================================
	GetVertexInputReflection
=================================================
*/
	static bool GetVertexInputReflection (const SpvReflectShaderModule &module, INOUT GraphicsPipelineDesc::VertexAttribs_t &attribs)
	{
		for (uint i = 0; i < module.input_variable_count; ++i)
		{
			auto&	src = module.input_variables[i];
			auto&	dst = attribs.emplace_back();

			ASSERT( src.name );

			dst.index	= src.location;
			dst.type	= FGEnumCastToVertexType( src.format );
			dst.id		= VertexID{ src.name };
			//dst.id	= VertexID( "attr"s << ToString(src.location) );
		}
		return true;
	}
	
/*
=================================================
	_GetFragmentOutputReflection
=================================================
*/
	static bool GetFragmentOutputReflection (const SpvReflectShaderModule &module, INOUT GraphicsPipelineDesc::FragmentOutputs_t &fragOutput)
	{
		for (uint i = 0; i < module.output_variable_count; ++i)
		{
			auto&	src = module.output_variables[i];
			auto&	dst = fragOutput.emplace_back();

			dst.type	= FGEnumCastToFragOutput( src.format );
			dst.index	= src.location;
		}
		return true;
	}
	
/*
=================================================
	GetReflection
=================================================
*/
	static bool GetReflection (const Array<uint> &spvShader, bool forceDBO, INOUT GraphicsPipelineDesc::FragmentOutputs_t &fragOutput,
								INOUT GraphicsPipelineDesc::VertexAttribs_t &attribs, INOUT PipelineDescription::PipelineLayout &layout)
	{
		SpvReflectShaderModule	module = {};
		CHECK_ERR( spvReflectCreateShaderModule( size_t(ArraySizeOf(spvShader)), spvShader.data(), OUT &module ) == SPV_REFLECT_RESULT_SUCCESS );

		if ( module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT ) {
			CHECK_ERR( GetVertexInputReflection( module, INOUT attribs ));
		}

		if ( module.shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT ) {
			CHECK_ERR( GetFragmentOutputReflection( module, INOUT fragOutput ));
		}

		for (uint i = 0; i < module.descriptor_set_count; ++i) {
			CHECK_ERR( GetDescriptorSetLayoutReflection( module.descriptor_sets[i], FGEnumCast(module.shader_stage), forceDBO, INOUT layout ));
		}

		for (uint i = 0; i < module.push_constant_block_count; ++i) {
			CHECK_ERR( GetPushConstantReflection( module.push_constant_blocks[i], FGEnumCast(module.shader_stage), INOUT layout ));
		}
		
		spvReflectDestroyShaderModule( &module );
		return true;
	}
	
/*
=================================================
	GetReflection
=================================================
*/
	static bool GetReflection (const Array<uint> &spvShader, bool forceDBO,
							   INOUT uint3 &localSize, INOUT uint3 &localSizeSpec, INOUT PipelineDescription::PipelineLayout &layout)
	{
		// TODO
		Unused( localSize, localSizeSpec );

		SpvReflectShaderModule	module = {};
		CHECK_ERR( spvReflectCreateShaderModule( size_t(ArraySizeOf(spvShader)), spvShader.data(), OUT &module ) == SPV_REFLECT_RESULT_SUCCESS );
		
		for (uint i = 0; i < module.descriptor_set_count; ++i) {
			CHECK_ERR( GetDescriptorSetLayoutReflection( module.descriptor_sets[i], FGEnumCast(module.shader_stage), forceDBO, INOUT layout ));
		}

		for (uint i = 0; i < module.push_constant_block_count; ++i) {
			CHECK_ERR( GetPushConstantReflection( module.push_constant_blocks[i], FGEnumCast(module.shader_stage), INOUT layout ));
		}
		
		spvReflectDestroyShaderModule( &module );
		return true;
	}

}	// FG
