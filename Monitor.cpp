#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;


// Несериализуемые данные
class Data {
    int data;
public:
    Data(const int& d = 0) { data = d; }
    Data(const Data& obj) = default;
    ~Data() = default;
    int getData() { return data; }
    void setData(const int& d) { data = d; }
    void printData() { cout << data << endl; }

    void addValue(int val) {
        cout << "Data is " << data << endl;
        data += val;
        cout << "Now data is " << data;
    }
};


// Монитор
class Monitor {
    // Критерий проверки наступления события
    bool checkEvent;

    // Несериализуемые данные 
    // передаваемые от потока-поставщика к потоку-потребителю
    Data* ptr;

    // Инструмент блокировки участков кода
    mutex mtx;

    // Возможность остановить выполнение потока-потребителя
    // до получения уведомления от потока-поставщика
    condition_variable cv;

    // Критерий остановки монитора
    bool isStopped;
public:
    Monitor() {
        checkEvent = false;
        ptr = nullptr;
        isStopped = false;
    }

    void provideData(Data* data); // Функция-поставщик
    void consumeData(); // Функция-потребитель

    void stopMonitor(); // Остановка монитора
    bool isStoppedCheck() const { return isStopped; }
};


void Monitor::stopMonitor() {
    unique_lock<mutex> ul(mtx, defer_lock);
    ul.lock();

    isStopped = true;

    // Все потоки должны проснуться
    cv.notify_all();

    ul.unlock();
}


// Функция поставщик
void Monitor::provideData(Data* data) {
    // Блокировка мьютекса
    unique_lock<mutex> ul(mtx, defer_lock);
    ul.lock();

    // Ожидание готовности
    while (checkEvent && !isStopped) {
        cout << "Provider: жду обработки предыдущего события" << endl;
        cv.wait(ul);
    }

    // Проверка не остановлен ли монитор
    if (isStopped) {
        cout << "Monitor is stopped\n";
        return;
    }

    // Выполнение действий для наступления события
    ptr = data;

    cout << "Provider: \n";
    ptr->addValue(10);
    cout << endl;

    // Уведомление потребителя
    checkEvent = true;
    cv.notify_one();

    // Освобождение мьютекса
    ul.unlock();
}

void Monitor::consumeData() {
    
    while (!isStopped) {
        // Блокировка мьютекса
        unique_lock<mutex> ul(mtx, defer_lock);
        ul.lock();

        // Ожидание события с временным освобождением мьютекса
        while (!checkEvent && !isStopped) {
            cout << "\nConsumer: жду событие\n\n";
            cv.wait(ul);
        }

        if (isStopped) break;

        // Обработка события
        cout << "\nConsumer: получил данные ";
        ptr->printData();

        // Уведомление о том, что можно отправить следующее событие
        checkEvent = false;
        cv.notify_one();

        // Освобождение мьютекса
        ul.unlock();
    }
    cout << "\nConsumer: exit\n";
}

// Функция для потока-поставщика
void providerFunction(Monitor* monitor, Data* data, int count) {
    for (int i = 0; i < count; i++) {
        // Задержка между событиями 1 секунда
        this_thread::sleep_for(chrono::seconds(1));
        monitor->provideData(data);
    }

    //this_thread::sleep_for(chrono::milliseconds(500));
    monitor->stopMonitor();
}

int main() {
    setlocale(LC_ALL, "Russian");
    Monitor monitor;
    Data data(100);

    cout << "MONITOR TEST\n\n";
    
    thread providerThread(providerFunction, &monitor, &data, 3);

    // this_thread::sleep_for(chrono::seconds(1));

    thread consumerThread(&Monitor::consumeData, &monitor);

    // Ждём завершения обоих потоков
    providerThread.join();
    consumerThread.join();

    cout << "\n\nEND\n" << endl;

    return 0;
}