//
// Created by noah on 2023/12/20.
//
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdint.h>

enum InfoState {
    Complete = -1,
    Normal = 0,
    Error = 1,
};

template<typename T>
struct Information {
    T data;
    char *error;
    InfoState state;
};

template<typename T>
class Emitter {
public:
    virtual void onPush(const T &data) = 0;

    virtual void onError(char *error) = 0;

    virtual void onComplete() = 0;
};

template<typename T>
class WorkOperator {
public:
    virtual void onWork(Emitter<T> &emitter) = 0;
};

template<typename T>
class Observer {
public:
    virtual void onAccept(T &data) = 0;

    virtual void onError(char *error) = 0;

    virtual void onComplete() = 0;
};

template<typename T>
class Consumer {
public:
    virtual void onAccept(T &data) = 0;
};

template<typename T>
class RxThread {
private:
    std::queue<Information<T>> queueSignal;
    Information<T> observableData;
    std::mutex tasksMutex;
    std::condition_variable condition;

    void onPush(const T &data) {
        observableData.state = Normal;
        observableData.data = data;
        queueSignal.push(observableData);
        condition.notify_one();
    }

    void onError(char *error) {
        observableData.state = Error;
        observableData.error = error;
        queueSignal.push(observableData);
        condition.notify_one();
    }

    void onComplete() {
        observableData.state = Complete;
        queueSignal.push(observableData);
        condition.notify_one();
    }

    class ObservableOnEmitter : public Emitter<T> {
    private:
        RxThread<T> *observablePtr;
    public:
        void setObservable(RxThread<T> *obs) {
            observablePtr = obs;
        }

        void onPush(const T &data) override {
            observablePtr->onPush(data);
        }

        void onError(char *error) override {
            observablePtr->onError(error);
        }

        void onComplete() override {
            observablePtr->onComplete();
        }
    };

    ObservableOnEmitter emitter;

    static void doWork(RxThread<T> *observable, WorkOperator<T> *onSubscribe) {
        onSubscribe->onWork(observable->emitter);
        observable->onComplete();
    }

    //执行观察线程
    static void doConsumerWork(RxThread<T> *observable, Consumer<T> *pConsumer) {
        while (true) {
            std::unique_lock<std::mutex> lock(observable->tasksMutex);
            // 等待任务到来
            observable->condition.wait(lock,
                                       [observable] { return !observable->queueSignal.empty(); });
            // 获取任务
            Information<T> signalData = observable->queueSignal.front();
            observable->queueSignal.pop();
            lock.unlock(); // 解锁，允许其他线程进入临界区
            // 如果任务是退出任务，则退出循环
            if (signalData.state == Complete) {
                return;
            }
            if (signalData.state == Normal) {
                // 在此处处理任务，例如执行函数或其他操作
                pConsumer->onAccept(signalData.data);
            }
        }
    }

    //执行观察线程
    static void doObserverWork(RxThread<T> *observable, Observer<T> *pObserver) {
        while (true) {
            std::unique_lock<std::mutex> lock(observable->tasksMutex);
            // 等待任务到来
            observable->condition.wait(lock,
                                       [observable] { return !observable->queueSignal.empty(); });
            // 获取任务
            Information<T> signalData = observable->queueSignal.front();
            observable->queueSignal.pop();
            lock.unlock(); // 解锁，允许其他线程进入临界区
            // 如果任务是退出任务，则退出循环
            if (signalData.state == Complete) {
                pObserver->onComplete();
                return;
            }
            if (signalData.state == Error) {
                pObserver->onError(signalData.error);
            } else {
                // 在此处处理任务，例如执行函数或其他操作
                pObserver->onAccept(signalData.data);
            }
        }
    }

public:
    RxThread() {
        emitter.setObservable(this);
    }

    void submit(WorkOperator<T> *pSubscriber) {
        std::thread subscribeWorker(doWork, this, pSubscriber);
        subscribeWorker.detach();
    }

    void subscribe(Observer<T> *pObserver) {
        std::thread observerWorker(doObserverWork, this, pObserver);
        observerWorker.detach();
    }

    void subscribe(Consumer<T> *pConsumer) {
        std::thread observerWorker(doConsumerWork, this, pConsumer);
        observerWorker.detach();
    }
};

