// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "VCmdBatch.h"

namespace FG
{

	//
	// Vulkan Submitted Batches
	//

	class VSubmitted final
	{
	// types
	public:
		static constexpr uint	MaxBatches		= 32;
		static constexpr uint	MaxSemaphores	= 8;

		using Batches_t			= FixedArray< VCmdBatchPtr, MaxBatches >;
		using Semaphores_t		= FixedArray< VkSemaphore, MaxSemaphores >;
		using Statistic_t		= IFrameGraph::Statistics;


	// variables
	private:
		const uint			_indexInPool;
		Batches_t			_batches;
		Semaphores_t		_semaphores;
		VkFence				_fence;
		EQueueType			_queueType;

		DataRaceCheck		_drCheck;


	// methods
	public:
		explicit VSubmitted (uint indexInPool);
		~VSubmitted ();

		// called by VFrameGraph
		void  Initialize (const VDevice &, EQueueType queue, ArrayView<VCmdBatchPtr>, ArrayView<VkSemaphore>);
		void  Release (const VDevice &, VDebugger &, const IFrameGraph::ShaderDebugCallback_t &, INOUT Statistic_t &);
		void  Destroy (const VDevice &);

		ND_ VkFence		GetFence ()			const	{ EXLOCK( _drCheck );  return _fence; }
		ND_ EQueueType	GetQueueType ()		const	{ EXLOCK( _drCheck );  return _queueType; }
		ND_ uint		GetIndexInPool ()	const	{ return _indexInPool; }
	};


}	// FG
