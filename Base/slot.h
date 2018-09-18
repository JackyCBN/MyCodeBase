#ifndef  KY_SLOT_H
#define  KY_SLOT_H

#include <functional>

namespace Ky
{
	template <typename T> class slot;

	template <class R, class ...Params>
	class slot<R(Params...)>
	{
	private:
		using Signature = R(Params&&...);

	public:
		slot() = default;

		/// Construct a slot with a static/global function call-back
		/// \param fn The static/global function
		slot(Signature fn)
			: _delegate(fn)
			, _connected(true)
		{}

		/// Construct a slot with a member-function
		/// \param obj The object that the member-function belongs to
		/// \param fn The member function of the object
		template <typename T, typename MemFnPtr>
		slot(T* obj, MemFnPtr fn)
			: _connected(true)
		{
			_delegate = std::bind(fn, obj);
		}

		slot(const std::function<Signature>& func)
			: _delegate(func)
			, _connected(true)
		{}

		//template <class ...Args>
		//slot(Args&&... args)
		//	: _connected(true)
		//{
		//	_delegate = std::bind(std::forward<Args>(args)...);
		//}
		/// Calls the slot
		/// \param args Any arguments you want to pass to the slot
		template <class ...Args>
		void operator()(Args&&... args) const
		{
			if (_connected && _delegate)
			{
				_delegate(std::forward<Args>(args)...);
			}
		}

		bool connected() const
		{
			return _connected;
		}

		void disconnect()
		{
			_connected = false;
		}

	private:

		/// The implementation of the slot, as a delegate.
		typedef std::function<Signature> impl_delegate;

		impl_delegate _delegate;

		bool _connected = false;
	};
}
#endif KY_SLOT_H