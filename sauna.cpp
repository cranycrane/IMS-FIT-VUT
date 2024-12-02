#include <simlib.h>
#include <iostream>
#include <getopt.h>
#include <stdexcept>

#define Norm(a, b) (std::max(0.0, Normal(a, b)))

// Global variables
const int SIMULATION_TIME = 8*60; // 8 hours
int customers_in = 0;  
int customers_out = 0; 

// Default values
double peak_arrivals = 6;  
double normal_arrivals = 8; 
int locker_count = 25;   
int shower_count = 3;    
int sauna_capacity = 15; 
int pool_capacity = 5;   
int lounger_count = 10;  

Store *lockers;
Store *showers;
Store *sauna;
Store *pool;
Store *rest_loungers;
Facility *reception;

Queue reception_queue("Fronta na recepci");
Queue sauna_queue("Fronta na saunu");
Queue pool_queue("Fronta na bazének");


// Histograms and statistics
Histogram arrivals_per_hour("Příchody za hodinu", 0, 60, 8); 

Stat waiting_lockers("Čekání na skříňky");
Stat waiting_reception("Čekání na recepci"); 
Stat waiting_sauna("Čekání na saunu"); 
Stat waiting_pool("Čekání na bazén"); 
Stat waiting_rest("Čekání na lehátko"); 

Histogram showers_skipped("Počty přeskočených sprch", 0, 60, 8);

// calculate arrival interval depending on peak/normal time
double ArrivalInterval(double currentTime) {
    if (currentTime < 3 * 60) { // 14:00-17:00
        return Exponential(normal_arrivals);
    } else if (currentTime >= 3 * 60) { // 17:00-20:00
        return Exponential(peak_arrivals); 
    } else {
        throw std::invalid_argument( "ArrivalInterval out of expected interval" );
    }
}

class Customer : public Process {
    double ArrivalTime; 
    const double MAX_TIME = 90; 
    double ExtraTime = 0;      

    void Behavior() override {
        customers_in++; 
        ArrivalTime = Time; 
        arrivals_per_hour(Time);

        // Lockers
        // Check free lockers and wait if no free
        double locker_wait_start_time = Time;
        while (lockers->Full()) {
            Wait(Uniform(1, 2)); 
        }
        double locker_wait_time = Time - locker_wait_start_time; 
        if (locker_wait_time >= 0) {
            waiting_lockers(locker_wait_time); // Log the waitting time
        } 


        // Reception
        if (reception->Busy()) {
            double queue_start_time = Time;
            reception_queue.Insert(this); // Go to queue
            Passivate();                 // Wait until free
            if (Time - queue_start_time > 0) {
                waiting_reception(Time - queue_start_time);
            } else {
                Print("Warning: Negative waiting time at time %f\n", Time);
            }
        }
        Seize(*reception);               
        Wait(Uniform(2, 3));             
        Release(*reception);             

        // Activate another customer in queue
        if (!reception_queue.Empty()) {
            Process *next_customer = static_cast<Process *>(reception_queue.GetFirst());
            next_customer->Activate();
        }

        // Enter in lockers
        Enter(*lockers, 1);
        Wait(Norm(5, 2)); 

        // Take a shower
        showersOrSkip(Norm(3, 3));

        // Lets start wellness procedure
        while (true) {
            double time_spent = Time - ArrivalTime;
            double remaining_time = MAX_TIME - time_spent;

            if (remaining_time < 15) {
                // Can decide to stay longer with 10% prob
                if (Random() < 0.10) { 
                    ExtraTime += remaining_time; 
                } else {
                    // Back to lockers and leaving system
                    Wait(Uniform(3, 6));
                    Leave(*lockers, 1);
                    break;
                }
            }

            PerformWellnessActivities();
        }

        customers_out++; 
    }

  void PerformWellnessActivities() {
        // Sauna
        if (sauna->Free() > 0) {
            sauna->Enter(this, 1);
        } else {
            double queue_start_time = Time;
            // Full, going to queue
            sauna_queue.Insert(this);
            Passivate(); 
            sauna->Enter(this, 1);
            waiting_sauna(Time - queue_start_time); // log waitting time
        }

        Wait(Norm(15,5)); // take a sauna
        sauna->Leave(1); 

        // active next in queue
        if (!sauna_queue.Empty()) {
            Process *next_customer = static_cast<Process *>(sauna_queue.GetFirst());
            next_customer->Activate();
        }

        double pool_or_shower = Random();

        // go to pool or shower with 50/50 prob
        if (pool_or_shower <= 0.5) // pool
        {
            // just a quick shower
            showersOrSkip(Norm(1, 1));

            if (pool->Free() > 0) {
                pool->Enter(this, 1);
            } else {
                double queue_start_time = Time;
                pool_queue.Insert(this);
                Passivate(); 
                pool->Enter(this, 1);
                waiting_pool(Time - queue_start_time);
            }

            Wait(Triag(2.5,0.5,5)); // triag waitting
            pool->Leave(1); 

            if (!pool_queue.Empty()) {
                Process *next_customer = static_cast<Process *>(pool_queue.GetFirst());
                next_customer->Activate();
            }
        }
        else // shower instead of pool
        {
            showersOrSkip(Norm(4, 2));
        }

        // Generate rest time
        double rest_time = Norm(12, 5);
        double standing_start_time = Time; 

        while (Time - standing_start_time < rest_time) {
            if (rest_loungers->Free() > 0) {
                // log waitting for loungers
                waiting_rest(Time - standing_start_time);

                rest_loungers->Enter(this, 1);
                double remaining_rest_time = rest_time - (Time - standing_start_time);
                if (remaining_rest_time > 0) {
                    Wait(remaining_rest_time); // finish rest time
                }
                rest_loungers->Leave(1);
                break;
            } else {
                // Standing and waitting until lounger free
                double remaining_stand_time = rest_time - (Time - standing_start_time);
                if (remaining_stand_time <= 0) {
                    // log the whole time standing
                    waiting_rest(Time - standing_start_time);
                    break; 
                }
                Wait(std::min(Uniform(0.1, 0.5), remaining_stand_time));
            }
        }

  }

  void showersOrSkip(double dtime) {
        if (showers->Free() >= 1)
        {
            Enter(*showers, 1);
            Wait(dtime); 
            Leave(*showers, 1);
        }
        else // showers full, skipping
        {
            showers_skipped(Time);
        }

  }
};

class Generator : public Event {
    void Behavior() override {
        // No visitors 90 minutes before end
        const double LAST_ENTRY_TIME = SIMULATION_TIME - 90;

        double nextArrivalInterval = ArrivalInterval(Time);
        if (Time + nextArrivalInterval < LAST_ENTRY_TIME) {
            (new Customer)->Activate();
            Activate(Time + nextArrivalInterval);
        } else {
            Print("Generátor ukončen: Čas=%f, Interval=%f\n", Time, nextArrivalInterval);
        }
    }
};


int main(int argc, char *argv[]) {
    // get command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:n:l:s:a:b:r:")) != -1) {
        switch (opt) {
            case 'p':
                peak_arrivals = std::stod(optarg);
                break;
            case 'n':
                normal_arrivals = std::stod(optarg);
                break;
            case 'l':
                locker_count = std::stoi(optarg);
                break;
            case 's':
                shower_count = std::stoi(optarg);
                break;
            case 'a':
                sauna_capacity = std::stoi(optarg);
                break;
            case 'b':
                pool_capacity = std::stoi(optarg);
                break;
            case 'r':
                lounger_count = std::stoi(optarg);
                break;
            default:
                std::cerr << "Neplatný argument\n";
                return 1;
        }
    }

    RandomSeed(time(NULL));

    lockers = new Store("Šatna", locker_count);
    showers = new Store("Sprchy", shower_count);
    sauna = new Store("Sauna", sauna_capacity);
    pool = new Store("Bazének", pool_capacity);
    rest_loungers = new Store("Odpočinková lehátka", lounger_count);
    reception = new Facility("Recepce");

    Init(0, SIMULATION_TIME); 

    (new Generator)->Activate();

    Run();

    // Outputs
    reception->Output();
    lockers->Output();  
    showers->Output();  
    sauna->Output();    
    pool->Output();     
    rest_loungers->Output();         
    arrivals_per_hour.Output(); 

    waiting_lockers.Output();   
    waiting_reception.Output(); 
    waiting_sauna.Output();    
    waiting_pool.Output();    
    waiting_rest.Output();    
    showers_skipped.Output();

    Print("Počet zákazníků, kteří vstoupili do systému: %d\n", customers_in);
    Print("Počet zákazníků, kteří opustili systém: %d\n", customers_out);

    return 0;
}
