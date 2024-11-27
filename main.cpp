#include <simlib.h>

// Konstanty a globální proměnné
const int SIMULATION_TIME = 480; // Simulace na 8 hodin (480 minut)

// Definice obslužných zařízení
Facility locker_room("Šatna");
Facility showers("Sprchy");
Facility sauna("Sauna");
Facility pool("Bazének");

// Histogramy a statistiky
Histogram arrivals_per_hour("Příchody za hodinu", 0, 60, 12); // Rozdělení příchodů
Histogram waiting_times("Čekací doby", 0, 1, 20); // Čekací doby

// Funkce pro výpočet průměrného intervalu mezi příchody
double ArrivalInterval(double currentTime) {
    if (currentTime >= 12 * 60 && currentTime < 14 * 60) { // 12:00 - 14:00
        return 20; // Delší interval
    } else if (currentTime >= 17 * 60 && currentTime < 20 * 60) { // 17:00 - 20:00
        return 5; // Kratší interval
    } else {
        return 10; // Standardní interval
    }
}

// Proces zákazníka
class Customer : public Process {
    double ArrivalTime; // Atribut pro uložení času příchodu

    void Behavior() override {
        ArrivalTime = Time; // Uložení času příchodu
        arrivals_per_hour(Time / 60); // Záznam příchodů podle hodin

        // Šatna
        Seize(locker_room);
        Wait(Exponential(5)); // Průměrná doba obsluhy: 5 minut
        Release(locker_room);

        // Sprchy
        Seize(showers);
        Wait(Exponential(5)); // Průměrná doba obsluhy: 5 minut
        Release(showers);

        // Sauna
        Seize(sauna);
        Wait(Exponential(15)); // Průměrná doba obsluhy: 15 minut
        Release(sauna);

        // Otužovací bazének
        Seize(pool);
        Wait(Exponential(5)); // Průměrná doba obsluhy: 5 minut
        Release(pool);

        // Záznam čekací doby
        waiting_times(Time - ArrivalTime);
    }
};

// Generátor příchodů zákazníků
class Generator : public Event {
    void Behavior() override {
        (new Customer)->Activate(); // Vytvoření nového zákazníka
        double nextArrivalInterval = Exponential(ArrivalInterval(Time)); // Dynamický interval
        Activate(Time + nextArrivalInterval); // Aktivace dalšího příchodu
    }
};

// Hlavní funkce simulace
int main() {
    // Inicializace simulace
    Init(0, SIMULATION_TIME); // Simulace od 0 do SIMULATION_TIME (480 minut)

    // Aktivace generátoru příchodů
    (new Generator)->Activate();

    // Spuštění simulace
    Run();

    // Výstupy
    locker_room.Output();  // Výstupy pro šatnu
    showers.Output();      // Výstupy pro sprchy
    sauna.Output();        // Výstupy pro saunu
    pool.Output();         // Výstupy pro bazének
    arrivals_per_hour.Output(); // Výstupy pro příchody za hodinu
    waiting_times.Output();     // Výstupy pro čekací doby

    return 0;
}
