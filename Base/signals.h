#ifndef SIGNAL_H
#define SIGNAL_H
#include <algorithm>
#include <vector>

#include "slot.h"
namespace Ky
{
	template <typename T> class signal;

	template <class R, class ...Params>
	class signal<R(Params...)>
	{
		using Signature = R(Params&&...);
	public:

		typedef slot<Signature> slot_type;

		/// Disconnects all slots from the signal
		void clear()
		{
			_slots.clear();
		}

		/// Connects a slot to the signal
		/// \param args The arguments you wish to construct the slot with to connect to the signal
		template <typename... Args>
		void connect(Args&&... args)
		{
			_slots.emplace_back(std::forward<Args>(args)...);
		}

		template <typename T, typename MemFnPtr>
		void connect(T* obj, MemFnPtr fn)
		{
			_slots.emplace_back(slot_type(obj,fn));
		}

		/// Disconnects a slot from the signal
		/// \param args The arguments you wish to construct a slot with
		template <typename... Args>
		void disconnect(Args&&... args)
		{
			auto it = std::find(_slots.begin(), _slots.end(), slot_type(std::forward<Args>(args)...));

			if (it != _slots.end())
			{
				_slots.erase(it);
			}
		}

		/// Emits the events you wish to send to the call-backs
		/// \param args The arguments to emit to the slots connected to the signal
		template <class ...Args>
		void emit(Args&&... args) const
		{
			for (typename slot_array::const_iterator i = _slots.begin(); i != _slots.end(); ++i)
			{
				(*i)(std::forward<Args>(args)...);
			}
		}

		/// Emits events you wish to send to call-backs
		/// \param args The arguments to emit to the slots connected to the signal
		/// \note
		/// This is equivalent to emit.
		template <class ...Args>
		void operator()(Args&&... args) const
		{
			emit(std::forward<Args>(args)...);
		}

		// comparison operators for sorting and comparing

		bool operator==(const signal& otherSignal) const
		{
			return _slots == otherSignal._slots;
		}

		bool operator!=(const signal& otherSignal) const
		{
			return !operator==(otherSignal);
		}

	private:

		/// defines an array of slots
		typedef std::vector<slot_type> slot_array;

		/// The slots connected to the signal
		slot_array _slots;
	};
}
#endif
