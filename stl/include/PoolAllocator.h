// Copyright (c)  Zhirnov Andrey. For more information see 'LICENSE.txt'

#pragma once

#include "stl/include/Math.h"
#include "stl/include/Bytes.h"

namespace FG
{

	//
	// Pool Allocator
	//

	struct PoolAllocator final
	{
	// types
	private:
		struct Block
		{
			void *		ptr			= null;
			size_t		size		= 0;		// used memory size
			size_t		capacity	= 0;		// size of block
		};


	// variables
	private:
		Array< Block >	_blocks;
		size_t			_blockSize	= 1024;


	// methods
	public:
		PoolAllocator ()
		{}

		~PoolAllocator ()
		{
			Clear();
		}


		void SetBlockSize (size_t size) noexcept
		{
			_blockSize = size;
		}

		void SetBlockSize (BytesU size) noexcept
		{
			_blockSize = size_t(size);
		}


		FG_ALLOCATOR void*  Alloc (size_t size, size_t align) noexcept
		{
			for (;;)
			{
				if ( not _blocks.empty() )
				{
					auto&	block	= _blocks.back();
					size_t	offset	= AlignToLarger( size_t(block.ptr) + block.size, align ) - size_t(block.ptr);

					if ( size <= (block.capacity - offset) )
					{
						block.size = offset + size;
						return block.ptr + BytesU(offset);
					}
				}

				_blocks.push_back(Block{ ::operator new( _blockSize, std::nothrow_t() ), 0, _blockSize });
			}
		}
		

		FG_ALLOCATOR void*  Alloc (BytesU size, BytesU align) noexcept
		{
			return Alloc( size_t(size), size_t(align) );
		}


		template <typename T>
		FG_ALLOCATOR void*  Alloc () noexcept
		{
			return Alloc( sizeof(T), alignof(T) );
		}


		void Clear () noexcept
		{
			for (auto& block : _blocks) {
				::operator delete( block.ptr, std::nothrow_t() );
			}
			_blocks.clear();
		}
	};


}	// FG