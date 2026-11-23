// Pin du transistor qui commande le buzzer
const int buzzerPin = 12;

// Petite mélodie (fréquences en Hz)
int melody[] = {
  523, // DO
  659, // MI
  784, // SOL
  1047 // DO aigu
};

// Durée des notes (ms)
int durations[] = {
  200, 200, 200, 400
};

int notes = 4;

void setup() {
  pinMode(buzzerPin, OUTPUT);
}

void loop() {

  for (int i = 0; i < notes; i++) {
    tone(buzzerPin, melody[i]);     // joue la note
    delay(durations[i]);            // durée
    noTone(buzzerPin);              // stop
    delay(50);                      // petite pause
  }

  delay(2000); // pause avant de rejouer la mélodie
}