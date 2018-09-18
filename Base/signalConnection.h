#ifndef SIGNAL_CONNECTION_H
#define SIGNAL_CONNECTION_H
#include "slot.h"
#include "signals.h"
#include <memory>

namespace Ky
{
	template<class T> class SignalConnection;

	template <class R, class ...Params>
	class SignalConnection<R(Params...)>
	{
	public:
		using Signature = R(Params&&...);
		using Signal = signal<Signature>;
		using Slot = slot<Signature>;

		SignalConnection()
			: _signal(nullptr)
		{
		}

		SignalConnection(Signal& signal, const std::shared_ptr<Slot>& item)
			: _signal(&signal), _item(item)
		{
		}

		SignalConnection(const SignalConnection& other) = default;
		SignalConnection(SignalConnection&& other) = default;

		SignalConnection& operator=(const SignalConnection& other) = default;
		SignalConnection& operator=(SignalConnection&& other) = default;

		virtual ~SignalConnection()
		{
		}

		bool hasItem(const Slot& item) const
		{
			return _item.get() == &item;
		}

		bool connected() const
		{
			return _item->connected;
		}

		bool disconnect()
		{
			if (_signal && _item && _item->connected())
			{
				return _signal->disconnect(*this);
			}
			return false;
		}
	private:
		Signal _signal;
		std::shared_ptr<Slot> _item;
	};
}

#endif
