#include <TFT_eSPI.h> // Libreria per il display
#include <SPI.h>
#include <wiring_private.h>

// GROVE right port
// This disables internal I2C port, which is connected to internal 3-axis accelerometer
static Uart Serial3(&sercom4, D1, D0, SERCOM_RX_PAD_1, UART_TX_PAD_0);

// Crea l'oggetto per il display
TFT_eSPI tft = TFT_eSPI();

// ---------- CONFIGURAZIONI ----------
const int minPosition = 0;
const int maxPosition = 800;

// Definiamo un colore blu personalizzato.
// Puoi usare TFT_BLUE, o un codice a 16 bit tipo 0x001F, 0x03EF, ecc.

uint16_t selectedColor = TFT_CYAN; 

// Definiamo i passi disponibili per lo spostamento
int steps[] = {1, 5, 10, 50, 100};
const int numSteps = sizeof(steps) / sizeof(steps[0]);

// ---------- VARIABILI GLOBALI ----------
int currentNumber    = 0;   // Valore di spostamento scelto
int stepIndex        = 0;   // Indice del passo (per steps[])
int selectedArrow    = -1;  // -1=nessuna, 0=sinistra, 1=destra
int currentPosition  = 0;   // Posizione corrente (0..800)

// Macchina a stati
enum State {
  SELECTING_VALUE,
  SELECTING_DIRECTION,
  FINISHED
};
State currentState = SELECTING_VALUE;

// ---------------------------------------------------------------------------
// Ridisegna l'intero schermo in base allo stato corrente
// ---------------------------------------------------------------------------
void disegnaSchermoCompleto() {
  tft.fillScreen(TFT_BLACK);

  // Determiniamo il colore da usare per SPOSTAMENTO e PASSO
  // Se stiamo selezionando il valore => bianco
  // Se stiamo selezionando la direzione => grigio (TFT_DARKGREY)
  // Nota: "Posizione" e il valore di currentPosition restano SEMPRE blu
  uint16_t valueColor = (currentState == SELECTING_VALUE) ? selectedColor : TFT_DARKGREY;

  // -------------------------------------------------------------------------
  // 1) Disegno di "Posizione" e il suo valore
  // -------------------------------------------------------------------------
  // Testo "Posizione"
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); 
  // Coordinate per la scritta "Posizione", ad es. x=160, y=30
  tft.drawString("Posizione", 120, 30);

  // Valore di currentPosition
  // Stessa dimensione e colore
  tft.drawString(String(currentPosition), 280, 30);

  // -------------------------------------------------------------------------
  // 2) Disegno di "Spostamento" e del suo valore (currentNumber)
  //    Stessa dimensione e allineamento di "Posizione"
  //    Ma colore dipende da SELECTING_VALUE / SELECTING_DIRECTION
  // -------------------------------------------------------------------------
  // Scritta "Spostamento"
  tft.setTextSize(4);
  tft.setTextColor(valueColor, TFT_BLACK);
  // y=110 per la scritta
  tft.drawString("Spostamento", 160, 90);

  // Valore currentNumber
  tft.drawString(String(currentNumber), 160, 130);

  // -------------------------------------------------------------------------
  // 3) Disegno di "Passo: XX" (un po' più piccolo)  
  // -------------------------------------------------------------------------
  tft.setTextSize(2);
  tft.setTextColor(valueColor, TFT_BLACK);
  // y=190 per "Passo"
  tft.drawString("Passo: " + String(steps[stepIndex]), 160, 170);

  // -------------------------------------------------------------------------
  // 4) Disegno delle frecce in basso (y=220)
  // -------------------------------------------------------------------------
  // Se SELECTING_VALUE => frecce entrambe grigie
  // Se SELECTING_DIRECTION => 
  //   - selectedArrow = -1 => bianche
  //   - selectedArrow = 0 => sinistra verde, destra bianca
  //   - selectedArrow = 1 => sinistra bianca, destra verde
  uint16_t arrowLeftColor  = TFT_DARKGREY;  
  uint16_t arrowRightColor = TFT_DARKGREY;
  
  if (currentState == SELECTING_DIRECTION) {
    if (selectedArrow == -1) {
      arrowLeftColor  = selectedColor;
      arrowRightColor = selectedColor;
    }
    else if (selectedArrow == 0) {
      arrowLeftColor  = TFT_GREEN;
      arrowRightColor = selectedColor;
    }
    else if (selectedArrow == 1) {
      arrowLeftColor  = selectedColor;
      arrowRightColor = TFT_GREEN;
    }
  }

  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  int yFrecce = 220;

  // Freccia sinistra
  tft.setTextColor(arrowLeftColor, TFT_BLACK);
  tft.drawString("<---", 100, yFrecce);

  // Freccia destra
  tft.setTextColor(arrowRightColor, TFT_BLACK);
  tft.drawString("--->", 220, yFrecce);
}

// ---------------------------------------------------------------------------
// STATO: SELECTING_VALUE
// L'utente sceglie il valore di spostamento (currentNumber)
// ---------------------------------------------------------------------------
void updateSelectingValue() {
  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // AUMENTA spostamento
  if (rightPressed) {
    currentNumber += steps[stepIndex];
    disegnaSchermoCompleto();
    delay(200);
  }

  // DIMINUISCI spostamento
  if (leftPressed) {
    currentNumber -= steps[stepIndex];
    // Se vuoi evitare che currentNumber diventi negativo, puoi fare:
    // if (currentNumber < 0) currentNumber = 0;
    disegnaSchermoCompleto();
    delay(200);
  }

  // Step successivo (SU)
  if (upPressed) {
    if (stepIndex < numSteps - 1) {
      stepIndex++;
    }
    disegnaSchermoCompleto();
    delay(200);
  }

  // Step precedente (GIÙ)
  if (downPressed) {
    if (stepIndex > 0) {
      stepIndex--;
    }
    disegnaSchermoCompleto();
    delay(200);
  }

  // Conferma e passa a SELECTING_DIRECTION
  if (pressPressed) {
    currentState = SELECTING_DIRECTION;
    selectedArrow = -1; // all'inizio nessuna freccia selezionata
    disegnaSchermoCompleto();
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// STATO: SELECTING_DIRECTION
// L'utente sceglie la direzione (sinistra/destra) e aggiorna currentPosition
// con i vincoli [minPosition, maxPosition]. Ogni volta, invia G-code su seriale.
// ---------------------------------------------------------------------------
void updateSelectingDirection() {
  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // Se premi SU, torni a SELECTING_VALUE (per cambiare il valore di spostamento)
  if (upPressed) {
    currentState = SELECTING_VALUE;
    disegnaSchermoCompleto();
    delay(200);
    return;
  }

  // Freccia sinistra (decrementa posizione)
  if (leftPressed) {
    selectedArrow = 0;
    int newPos = currentPosition - currentNumber;
    if (newPos < minPosition) {
      newPos = minPosition;
    }
    currentPosition = newPos;

    // Invia su seriale il G-code in modalità assoluta
    Serial3.println("G90");      // imposta posizionamento assoluto
    Serial3.print("G0 X");       // movimento rapido verso X
    Serial3.println(currentPosition);

    disegnaSchermoCompleto();
    delay(200);
  }

  // Freccia destra (incrementa posizione)
  if (rightPressed) {
    selectedArrow = 1;
    int newPos = currentPosition + currentNumber;
    if (newPos > maxPosition) {
      newPos = maxPosition;
    }
    currentPosition = newPos;

    // Invia su seriale il G-code in modalità assoluta
    Serial3.println("G90");      
    Serial3.print("G0 X");
    Serial3.println(currentPosition);

    disegnaSchermoCompleto();
    delay(200);
  }

  // Se premi il tasto centrale, confermi e vai a FINISHED
  if (pressPressed) {
    //currentState = FINISHED;
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// STATO: FINISHED
// Mostra schermata finale e blocca il programma (o potresti fare altro).
// ---------------------------------------------------------------------------
void mostraSchermataFinale() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(10, 40);
  tft.println("Valore scelto:");

  tft.setTextSize(4);
  tft.setCursor(10, 90);
  tft.println(currentNumber);

  tft.setCursor(10, 150);
  tft.setTextSize(3);

  if (selectedArrow == 0) {
    tft.println("Direzione: SINISTRA");
  } else if (selectedArrow == 1) {
    tft.println("Direzione: DESTRA");
  } else {
    tft.println("Direzione: N/D");
  }

  // Mostriamo anche la "posizione" finale
  tft.setCursor(10, 200);
  tft.print("Posizione: ");
  tft.println(currentPosition);

  

  /*
  if (selectedArrow == 0) {
    Serial.println("Final Direction: SINISTRA");
  } else if (selectedArrow == 1) {
    Serial.println("Final Direction: DESTRA");
  } else {
    Serial.println("Final Direction: N/D");
  }*/
  
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
  // Inizializza seriale 
  Serial3.begin(115200);
    pinPeripheral(D0, PIO_SERCOM_ALT);
    pinPeripheral(D1, PIO_SERCOM_ALT);
  delay(1000); // piccola pausa
  
  // Inizializza il display
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Impostazione pin per i pulsanti
  pinMode(WIO_KEY_A,   INPUT_PULLUP);
  pinMode(WIO_KEY_B,   INPUT_PULLUP);
  pinMode(WIO_KEY_C,   INPUT_PULLUP);

  pinMode(WIO_5S_UP,    INPUT_PULLUP);
  pinMode(WIO_5S_DOWN,  INPUT_PULLUP);
  pinMode(WIO_5S_LEFT,  INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  // Disegno iniziale
  disegnaSchermoCompleto();
}

// ---------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------
void loop() {
  switch (currentState) {
    case SELECTING_VALUE:
      updateSelectingValue();
      break;

    case SELECTING_DIRECTION:
      updateSelectingDirection();
      break;

    case FINISHED:
      // Mostra schermata finale una sola volta
      mostraSchermataFinale();
      // Rimani qui
      while(true) {
        delay(100);
      }
  }
}




// ---------------------------------------------------------------------------
// PER SERIALE
// ---------------------------------------------------------------------------


void SERCOM4_0_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_1_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_2_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_3_Handler()
{
  Serial3.IrqHandler();
}


