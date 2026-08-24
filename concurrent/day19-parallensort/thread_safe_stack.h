#pragma once
#include <stack>
#include <mutex>
#include <condition_variable>

template<typename  T>
class thread_safe_stack
{
private:
	std::stack<T> data;
	mutable std::mutex m;
	std::condition_variable cv;
public:
	thread_safe_stack() {}

	thread_safe_stack(const thread_safe_stack& other)
	{
		std::lock_guard<std::mutex> lock(other.m);
		data = other.data;
	}

	thread_safe_stack& operator=(const thread_safe_stack&) = delete;

	void push(T new_value)
	{
		std::lock_guard<std::mutex> lock(m);
		data.push(std::move(new_value));    // ⇽-- - ①
		cv.notify_one();
	}

	std::shared_ptr<T> wait_and_pop()
	{
		std::unique_lock<std::mutex> lock(m);
		cv.wait(lock, [this]()   //  ⇽-- - ②
			{
				if (data.empty())
				{
					return false;
				}
				return true;
			});


		std::shared_ptr<T> const res(
			std::make_shared<T>(std::move(data.top())));   // ⇽-- - ③
		data.pop();   // ⇽-- - ④
		return res;
	}

	void wait_and_pop(T& value)
	{
		std::unique_lock<std::mutex> lock(m);
		cv.wait(lock, [this]()
			{
				if (data.empty())
				{
					return false;
				}
				return true;
			});

		value = std::move(data.top());   // ⇽-- - ⑤
		data.pop();   // ⇽-- - ⑥
	}

	bool empty() const
	{
		std::lock_guard<std::mutex> lock(m);
		return data.empty();
	}

	bool try_pop(T& value)
	{
		std::lock_guard<std::mutex> lock(m);
		if (data.empty())
		{
			return false;
		}

		value = std::move(data.top());
		data.pop();
		return true;
	}

	std::shared_ptr<T> try_pop()
	{
		std::lock_guard<std::mutex> lock(m);
		if (data.empty())
		{
			return std::shared_ptr<T>();
		}

		std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
		data.pop();
		return res;
	}

};