# Cpp-RxThread
一个c++版的线程管理器，开启线程处理的过程中，可以订阅该工作线程push的信息，并在另外一个订阅线程中处理，不耽误工作线程的执行。
使用方法：

1.先定义工作线程的需要交互的数据信息类型，并继承WorkOperator重写onWork方法，如：

    class InputWork : public WorkOperator<int> {
    public:
        void onWork(Emitter<int> &emitter) override {
            for (int i = 0; i < 20; ++i) {
                if (i == 10) {
                    //捕获错误日志信息
                    emitter.onError("发现错误");
                } else {
                    //提交数据信息给另外的现场处理
                    emitter.onPush(i);
                }
                sleep(1);
            }
            //完成订阅线程的处理，订阅线程关闭
            emitter.onComplete();
            //如果在onComplete之后继续push信息，订阅线程已关闭，不再订阅该信息。
            emitter.onPush(i);
            //当工作线程完成的时候，订阅线程同步完成，自动关闭
        }
    };
    
2.定义订阅处理器 继承Observer并重写其中的方法

    class InputObserver : public Observer<int> {
    public:
        void onAccept(int &data) override {
            //订阅线程，接收到工作线程提交过来的数据，这是另外的线程来处理的
        }
    
        void onError(char *error) override {
            //订阅线程，接收到工作线程提交过来的错误信息
        }
    
        void onComplete() override {
            //订阅线程已完成工作，关闭线程。
        }
    };
    
或者继承重写Consumer

    class InputConsumer : public Consumer<int> {
    public:
        void onAccept(int &data) override {
             //订阅线程，接收到工作线程提交过来的数据，这是另外的线程来处理的;Consumer只有接收的方法，没有订阅错误以及完成的事件
        }
    };

3.在class中创建RxThread，WorkOperator，Observer，然后开始使用

      class InputManager {
          RxThread<int> observable;//创建RxThread的实现，注意不要在方法中直接创建对象，如果该对象的生命周期被清除了，工作线程还未完成，会异常崩溃。
          InputWork onSubscribe;//工作线程
          InputObserver observer;//订阅线程
          InputConsumer consumer;//订阅线程
      public:
          void onStart() {
              //要注意RxThread的生命周期要比工作线程长。
              observable.submit(&onSubscribe);//提交工作任务
              observable.subscribe(&observer);//注册订阅
              //observable.subscribe(&consumer);//注意Observer跟Consumer两者都注册的话，一次只能有一个订阅线程抢到事件，另外的线程无法接收到该次事件。
          }
      
      };

