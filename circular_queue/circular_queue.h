#include <mutex>
#include <vector>

using namespace std;

template <typename T, size_t cap>
class CircularQueue {
    public:
    CircularQueue() : data(cap + 1){}

    bool push(const T& val){
        lock_guard<mutex> lock(mtx);
        if((tail + 1) % data.size() == head) return false; //满
        data[tail] = val;
        tail = (tail + 1) % data.size();
        return true;
    }

    bool pop(T& val) {
        lock_guard<mutex> lock(mtx);
        if(head == tail) return false;//空
        val = std::move(data[head]);
        head = (head + 1) % data.size();
        return true;
    }

    bool empty() {
        lock_guard<mutex> lock(mtx);
        return head == tail;
    }

    bool full() {
        lock_guard<mutex> lock(mtx);
        return (tail + 1) % data.size() == head;
    }

    private:
        vector<T> data;
        mutex mtx;
        size_t head = 0;
        size_t tail = 0;
};
