// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "scene/Loader/Intermediate/IntermMesh.h"
#include "framegraph/Shared/EnumUtils.h"

namespace FG
{

/*
=================================================
	constructor
=================================================
*/
	IntermMesh::IntermMesh (Array<uint8_t> &&vertices, const VertexAttributesPtr &attribs,
							BytesU vertStride, EPrimitive topology,
							Array<uint8_t> &&indices, EIndex indexType) :
		_vertices{ std::move(vertices) },	_attribs{ attribs },
		_vertexStride{ vertStride },		_topology{ topology },
		_indices{ std::move(indices) },		_indexType{ indexType }
	{}

/*
=================================================
	CalcAABB
=================================================
*/
	void IntermMesh::CalcAABB ()
	{
		CHECK_ERRV( _attribs and _vertexStride > 0 and _vertices.size() );

		_boundingBox = AABB{};

		StructView<vec3>	positions = _attribs->GetData<vec3>( EVertexAttribute::Position, _vertices.data(),
																 GetVertexCount(), _vertexStride );
		if ( positions.empty() )
			return;

		AABB	bbox{ positions[0] };

		for (size_t i = 1; i < positions.size(); ++i)
		{
			bbox.Add( positions[i] );
		}

		_boundingBox = bbox;
	}
	
/*
=================================================
	GetIndexStride
=================================================
*/
	BytesU  IntermMesh::GetIndexStride () const
	{
		return EIndex_SizeOf(_indexType);
	}

}	// FG
