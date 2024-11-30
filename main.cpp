#include <simlib.h>
#include <stdexcept>

#define Norm(a, b) (std::max(0.0, Normal(a, b)))

// Konstanty a globální proměnné
const int SIMULATION_TIME = 8*60; // Simulace na 8 hodin
int customers_in = 0;  // Počet zákazníků, kteří vstoupili do systému
int customers_out = 0; // Počet zákazníků, kteří systém opustili

// Definice obslužných zařízení
Facility reception("Recepce");

Store lockers("Šatna", 25);
Store showers("Sprchy", 3);
Store sauna("Sauna", 12);
Store pool("Bazének", 5);
Store rest_loungers("Odpočinková lehátka", 8);

Queue reception_queue("Fronta na recepci");
Queue sauna_queue("Fronta na saunu");
Queue pool_queue("Fronta na bazének");


// Histogramy a statistiky
Histogram arrivals_per_hour("Příchody za hodinu", 0, 60, 8); // Rozdělení příchodů

Stat waiting_lockers("Čekání na skříňky");
Stat waiting_reception("Čekání na recepci"); // Čekací doby
Stat waiting_sauna("Čekání na saunu"); // Čekací doby
Stat waiting_pool("Čekání na bazén"); // Čekací doby
Stat waiting_rest("Čekání na lehátko"); // Čekací doby

Histogram showers_skipped("Počty přeskočených sprch", 0, 60, 8); // Intervaly 60 minut, 16 intervalů pro 16 hodin

// Funkce pro výpočet průměrného intervalu mezi příchody
double ArrivalInterval(double currentTime) {
    if (currentTime < 3 * 60) { // 14:00-17:00
        return Exponential(8);
    } else if (currentTime >= 3 * 60) { // 17:00-20:00
        return Exponential(6); 
    } else {
        throw std::invalid_argument( "ArrivalInterval out of expected interval" );
    }
}

// Proces zákazníka
class Customer : public Process {
    double ArrivalTime; // Atribut pro uložení času příchodu
    const double MAX_TIME = 90; // Limit času (v minutách)
    double ExtraTime = 0;      // Přesčasový čas (v minutách)

    void Behavior() override {
        customers_in++; // Zákazník vstoupil do systému
        ArrivalTime = Time; // Uložení času příchodu
        arrivals_per_hour(Time);

        // Kontrola volných skříněk
        double locker_wait_start_time = Time; // Začátek čekání na volnou skříňku
        while (lockers.Full()) {
            Wait(Uniform(1, 2)); // Zákazník čeká, než se skříňka uvolní
        }
        double locker_wait_time = Time - locker_wait_start_time; // Celkový čas čekání na skříňku
        if (locker_wait_time >= 0) {
            waiting_lockers(locker_wait_time); // Záznam čekací doby
        } else {
            Print("Warning: Negative locker wait time at time %f, waitTime: %f\n", Time, locker_wait_time);
        }


        // Recepce
        if (reception.Busy()) {
            double queue_start_time = Time;
            reception_queue.Insert(this); // Zařadíme do fronty
            Passivate();                 // Čekáme na uvolnění
            if (Time - queue_start_time > 0) {
                waiting_reception(Time - queue_start_time);
            } else {
                Print("Warning: Negative waiting time at time %f\n", Time);
            }
        }
        Seize(reception);               // Obsazení recepce
        Wait(Norm(2, 1));             // Simulace obsluhy
        Release(reception);             // Uvolnění recepce

        // Aktivujeme dalšího zákazníka z fronty, pokud existuje
        if (!reception_queue.Empty()) {
            Process *next_customer = static_cast<Process *>(reception_queue.GetFirst());
            next_customer->Activate();
        }


        // Šatna
        Enter(lockers, 1);
        Wait(Norm(5, 2)); 

        // Sprchy
        showersOrSkip(Norm(3, 3));

        while (true) {
            // Zjistíme zbývající čas
            double time_spent = Time - ArrivalTime;
            double remaining_time = MAX_TIME - time_spent;

            if (remaining_time < 15) {
                // Rozhodování o pokračování
                if (Random() < 0.10) { // 30% pravděpodobnost, že zůstane déle
                    ExtraTime += remaining_time; // Zákazník se rozhodl zůstat
                } else {
                    // Odchod do šatny a opuštění systému
                    Wait(Uniform(3, 6));
                    Leave(lockers, 1);
                    break;
                }
            }

            // Kolečko wellness aktivit
            PerformWellnessActivities();
        }



        // Zákazník opouští systém
        customers_out++; // Zákazník opustil systém
        // waiting_times(Time - ArrivalTime); // Záznam čekací doby
    }

  void PerformWellnessActivities() {
        // Sauna
        if (sauna.Free() > 0) {
            // Kapacita volná, vstup přímo do sauny
            sauna.Enter(this, 1);
        } else {
            double queue_start_time = Time;
            // Kapacita plná, zařadit do fronty
            sauna_queue.Insert(this);
            Passivate(); // Čekání na volné místo
            sauna.Enter(this, 1);
            waiting_sauna(Time - queue_start_time);

        }

        Wait(Norm(15,5)); // Průměrná doba obsluhy: 15 minut
        sauna.Leave(1); // Uvolnění kapacity

        // Pokud je někdo ve frontě, aktivovat dalšího
        if (!sauna_queue.Empty()) {
            Process *next_customer = static_cast<Process *>(sauna_queue.GetFirst());
            next_customer->Activate();
        }


        double pool_or_shower = Random();

        // rozhoduje se zda se ochladí v bazénku nebo sprše
        if (pool_or_shower <= 0.5) // bazenek
        {
            showersOrSkip(Norm(1, 1));

            // Bazének
            if (pool.Free() > 0) {
                // Kapacita volná, vstup přímo do sauny
                pool.Enter(this, 1);
            } else {
                double queue_start_time = Time;
                // Kapacita plná, zařadit do fronty
                pool_queue.Insert(this);
                Passivate(); // Čekání na volné místo
                pool.Enter(this, 1);
                waiting_pool(Time - queue_start_time);
            }

            Wait(Triag(2.5,0.5,5)); // Průměrná doba obsluhy: 5 minut
            pool.Leave(1); // Uvolnění kapacity

            // Pokud je někdo ve frontě, aktivovat dalšího
            if (!pool_queue.Empty()) {
                Process *next_customer = static_cast<Process *>(pool_queue.GetFirst());
                next_customer->Activate();
            }
        }
        else // sprcha
        {
            showersOrSkip(Norm(4, 2));
        }

        // Generování času odpočinku
        double rest_time = Norm(12, 5);
        double standing_start_time = Time; // Začátek stání

        while (Time - standing_start_time < rest_time) {
            if (rest_loungers.Free() > 0) {
                // Zaznamenání doby čekání do statistiky
                waiting_rest(Time - standing_start_time);

                // Volné lehátko, zákazník si ho zabere
                rest_loungers.Enter(this, 1);
                double remaining_rest_time = rest_time - (Time - standing_start_time);
                if (remaining_rest_time > 0) {
                    Wait(remaining_rest_time); // Dokončení odpočinku na lehátku
                }
                rest_loungers.Leave(1); // Uvolnění lehátka
                break;
            } else {
                // Stojí a čeká, ale jen dokud má smysl
                double remaining_stand_time = rest_time - (Time - standing_start_time);
                if (remaining_stand_time <= 0) {
                    // Zaznamenání celkové doby čekání (bez lehátka)
                    waiting_rest(Time - standing_start_time);
                    break; // Pokud už není čas na další čekání
                }
                Wait(std::min(Uniform(0.1, 0.5), remaining_stand_time));
            }
        }

  }

  void showersOrSkip(double dtime) {
        if (showers.Free() >= 1)
        {
            Enter(showers, 1);
            Wait(dtime); 
            Leave(showers, 1);
        }
        else // sprchy obsazene, skipuju
        {
            showers_skipped(Time);
        }

  }
};

// Generátor příchodů zákazníků
class Generator : public Event {
    void Behavior() override {
        // Spočítáme čas pro poslední možný příchod
        const double LAST_ENTRY_TIME = SIMULATION_TIME - 90; // 90 minut před koncem simulace

        double nextArrivalInterval = ArrivalInterval(Time); // Generujeme kladný interval
        if (Time + nextArrivalInterval < LAST_ENTRY_TIME) {
            (new Customer)->Activate();
            Activate(Time + nextArrivalInterval); // Naplánujeme další příchod
        } else {
            Print("Generátor ukončen: Čas=%f, Interval=%f\n", Time, nextArrivalInterval);
        }
    }
};


// Hlavní funkce simulace
int main() {
    RandomSeed(time(NULL));
    // Inicializace simulace
    Init(0, SIMULATION_TIME); // Simulace od 0 do SIMULATION_TIME (960 minut)

    // Aktivace generátoru příchodů
    (new Generator)->Activate();

    // Spuštění simulace
    Run();

    // Výstupy
    reception.Output();
    lockers.Output();  // Výstupy pro šatnu
    showers.Output();      // Výstupy pro sprchy
    sauna.Output();        // Výstupy pro saunu
    pool.Output();         // Výstupy pro bazének
    arrivals_per_hour.Output(); // Výstupy pro příchody za hodinu

    waiting_lockers.Output();     // Výstupy pro čekací doby
    waiting_reception.Output();     // Výstupy pro čekací doby
    waiting_sauna.Output();     // Výstupy pro čekací doby
    waiting_pool.Output();     // Výstupy pro čekací doby
    waiting_rest.Output();     // Výstupy pro čekací doby
    showers_skipped.Output();

    // Výpis počtu zákazníků
    Print("Počet zákazníků, kteří vstoupili do systému: %d\n", customers_in);
    Print("Počet zákazníků, kteří opustili systém: %d\n", customers_out);

    return 0;
}
