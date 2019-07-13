#include <pthread.h>  
#include <iostream>
#include <vector>
#include <functional>

using namespace std;

template <class T>
class Holder {
    T data;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
public:
    Holder(T data){
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);
        this->set(data);
    }

    T get(){
        pthread_mutex_lock(&mutex);
        T data = this->data;
        pthread_mutex_unlock(&mutex);
        return data;
    }

    void set(T data){
        pthread_mutex_lock(&mutex);
        this->data = data;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }

    void wait(function<bool(T)> predicate){
        pthread_mutex_lock(&mutex);
        while(!predicate(this->data)){
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);
    }

    ~Holder(){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }


};

struct Data {
    int data;
    bool is_updated;
    bool time_to_finish;

    Data(){
        this->data = 0;
        this->is_updated = false;
        this->time_to_finish = false;
    }

    Data(int data, bool is_updated){
        this->data = data;
        this->is_updated = is_updated;
        this->time_to_finish = false;
    }

    Data(int data, bool is_updated, bool time_to_finish){
        this->data = data;
        this->is_updated = is_updated;
        this->time_to_finish = time_to_finish;
    }
};

Holder<Data> new_data = Holder<Data>(Data());
Holder<bool> is_data_recieved = Holder<bool>(false);
Holder<bool> is_consumer_started = Holder<bool>(false);
Holder<bool> is_producer_finished = Holder<bool>(false);

void* producer_routine(void* arg) {
    // Wait for consumer to start

    is_consumer_started.wait([](bool started) { return started; });

    bool exit = false;
    int v;

    while(!exit){
        is_data_recieved.set(false);

        if(!(cin >> v)){
            exit = true;
            is_producer_finished.set(true);

            // Time to finish consumer
            new_data.set(Data(0, true, true));
        } else {
            new_data.set(Data(v, true));
        }

        is_data_recieved.wait([] (bool received) { return received; });
    }
}

void* consumer_routine(void* arg) {
    // notify about start
    // allocate value for result
    // for every update issued by producer, read the value and add to sum
    // return pointer to result

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

    is_consumer_started.set(true);

    int sum = 0;
    int exit = false;

    while(!exit){
        new_data.wait([] (Data data) { return data.is_updated; });

        Data data = new_data.get();

        if(data.time_to_finish){
            exit = is_producer_finished.get();
        } else {
            sum += new_data.get().data;
        }

        new_data.set(Data(0, false));
        is_data_recieved.set(true);

    }

    return new int(sum);
}

void* consumer_interruptor_routine(void* arg) {
    // wait for consumer to start
    is_consumer_started.wait([](bool started) { return started; });

    // interrupt consumer while producer is running
    pthread_t* consumer = (pthread_t *)arg;

    bool exit = false;

    while(!exit){
        pthread_cancel(*consumer);

        exit = is_producer_finished.get();
    }
}

int run_threads() {
    // start 3 threads and wait until they're done
    // return sum of update values seen by consumer

    int* sum = 0;

    pthread_t producer;
    pthread_t consumer;
    pthread_t interruptor;

    pthread_create(&consumer, nullptr, consumer_routine, nullptr);
    pthread_create(&interruptor, nullptr, consumer_interruptor_routine, &consumer);
    pthread_create(&producer, nullptr, producer_routine, nullptr);

    pthread_join(consumer, (void **)&sum);
    pthread_join(interruptor, nullptr);
    pthread_join(producer, nullptr);

    return *sum;
}

int main() {
    std::cout << run_threads() << std::endl;
    return 0;
}
