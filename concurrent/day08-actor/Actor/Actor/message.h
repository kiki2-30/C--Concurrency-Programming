#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
namespace messaging
{
	//①消息基类。队列中存储的项目
	struct message_base   
	{
		virtual ~message_base()
		{}
	};

	//②每种消息都具有特化类型
	template<typename Msg>
	struct wrapped_message : 
		message_base
	{
		Msg contents;
		explicit wrapped_message(Msg const& contents_) :
			contents(contents_)
		{}
	};

	//③消息队列
	class queue   
	{
		std::mutex m;
		std::condition_variable c;
		//④以内部队列存储message_base型共享指针
		std::queue<std::shared_ptr<message_base> > q;    
	public:
		template<typename T>
		void push(T const& msg)
		{
			std::lock_guard<std::mutex> lk(m);
			//⑤包装发布的消息，并存储相关的指针
			q.push(std::make_shared<wrapped_message<T> >(msg));     
			c.notify_all();
		}
		std::shared_ptr<message_base> wait_and_pop()
		{
			std::unique_lock<std::mutex> lk(m);
			//⑥如果队列为空，就发生阻塞
			c.wait(lk,[&] {return !q.empty(); });   
			auto res = q.front();
			q.pop();
			return res;
		}
	};
}
