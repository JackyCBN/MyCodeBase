#ifndef SCOPED_SIGNAL_CONNECTION_H
#define SCOPED_SIGNAL_CONNECTION_H
#include "slot.h"
#include "signals.h"
#include <memory>
#include "signalConnection.h"

namespace Ky
{
	template<class T> class scopedSignalConnection;

	template <class R, class ...Params>
	class scopedSignalConnection<R(Params...)> 
		: public SignalConnection<R(Params...)>
	{
	public:
		using Signature = R(Params&&...);
		using Signal = signal<Signature>;
		using Slot = slot<Signature>; 
		using SignalConnection = SignalConnection<Signature>;

		scopedSignalConnection()
		{
		}

		scopedSignalConnection(Signal signal, const std::shared_ptr<Slot>& item)
			: SignalConnection(signal, item)
		{
		}

		scopedSignalConnection(const Signal& other)
			: SignalConnection(other)
		{
		}

		scopedSignalConnection(const scopedSignalConnection& other) = delete;
		scopedSignalConnection(scopedSignalConnection&& other) = delete;

		scopedSignalConnection& operator=(const scopedSignalConnection& other) = delete;
		scopedSignalConnection& operator=(scopedSignalConnection&& other) = delete;

		~scopedSignalConnection()
		{
			disconnect();
		}
	};
}

#endif
