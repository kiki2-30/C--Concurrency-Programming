#include <atomic>
#include <memory>
#include <utility>

using namespace std;

template<typename T , size_t Cap>
class CircularQueSeq {
    public:
        CircularQueSeq() : cap(Cap + 1) , data(allocator<T>().allocate(cap)) {

        }
        CircularQueSeq(const CircularQueSeq&) = delete;
        CircularQueSeq& operator=(const CircularQueSeq&) = delete;

        ~CircularQueSeq() {
            while(head != tail) {
                std::destroy_at(data + head);
                head = (head + 1) % cap;
            }
            allocator<T>().deallocate(data, cap);
        }
        
        bool push(const T& val) {
            lock();
            if ((tail + 1) % cap == head) {   // 满
                unlock();
                return false;
            }
            std::construct_at(data + tail, val);   // 拷贝构造
            tail = (tail + 1) % cap;
            unlock();
            return true;
        }

        bool pop(T& val) {
            lock();
            if (head == tail) {   // 空
                unlock();
                return false;
            }
            val = std::move(data[head]);
            std::destroy_at(data + head);   // 取出后销毁旧对象
            head = (head + 1) % cap;
            unlock();
            return true;
        }


    private:
        void lock() {
            //"拿到锁之后，锁里面的代码要按顺序执行，不能偷偷跑到锁外面去。"
            while(using_.exchange(true, memory_order_acquire)) {

            }
        }

        void unlock() {
            //因为只有持有锁的线程才能调用 unlock()，不需要原子交换，直接赋值即可。
            //store 比 exchange 性能更好（不需要读取旧值）。
            using_.store(false, memory_order_release);
        }

        size_t cap;
        T* data;
        atomic<bool> using_{false};
        size_t head = 0;
        size_t tail = 0;
};