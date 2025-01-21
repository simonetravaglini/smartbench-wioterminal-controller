#include <TFT_eSPI.h> // Libreria per il display
#include <SPI.h>
#include <wiring_private.h>

// ------------------- SERIALE SU GROVE Right Port -------------------
static Uart Serial3(&sercom4, D1, D0, SERCOM_RX_PAD_1, UART_TX_PAD_0);

// Crea l'oggetto per il display
TFT_eSPI tft = TFT_eSPI();

// ------------------- CONFIGURAZIONI -------------------
const int minPosition = 0;
const int maxPosition = 800;
uint16_t selectedColor = TFT_CYAN; 

int steps[] = {1, 5, 10, 50, 100};
const int numSteps = sizeof(steps) / sizeof(steps[0]);

// ------------------- VARIABILI GLOBALI -------------------
bool showMenu       = true;  
int  menuIndex      = 0;     
String menuItems[4] = {"jogging", "positioning", "joint", "settings"};

// Variabili per la modalità Jogging
int currentNumber    = 0;   
int stepIndex        = 0;   
int selectedArrow    = -1;  
int currentPosition  = 0;   

enum JoggingState {
  SELECTING_VALUE,
  SELECTING_DIRECTION,
  FINISHED
};
JoggingState currentState = SELECTING_VALUE;

// *** NUOVO: positioning ***
bool positioningMode = false;  // Flag per indicare se siamo in "positioning"
int  newPosition     = 0;      // Posizione target da impostare

// ---------------------------------------------------------------------------
// DISEGNO DEL MENU
// ---------------------------------------------------------------------------
void drawMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  tft.setTextDatum(TL_DATUM); 

  int spacing = 40;
  int startY  = 40;

  for (int i = 0; i < 4; i++) {
    // freccia > se è l’elemento selezionato
    String line = (i == menuIndex) ? ("> " + menuItems[i]) : ("  " + menuItems[i]);
    uint16_t color = (i == menuIndex) ? selectedColor : TFT_WHITE;
    
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(line, 40, startY + i * spacing);
  }
}

// ---------------------------------------------------------------------------
// GESTIONE INPUT JOYSTICK NEL MENU
// ---------------------------------------------------------------------------
void updateMenu() {
  bool upPressed    = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed  = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool pressPressed = (digitalRead(WIO_5S_PRESS) == LOW);

  // Navigazione su/giù
  if (upPressed) {
    if (menuIndex > 0) {
      menuIndex--;
      drawMenu();
      delay(200);
    }
  }
  if (downPressed) {
    if (menuIndex < 3) {
      menuIndex++;
      drawMenu();
      delay(200);
    }
  }

  // Se si preme il pulsante centrale si conferma la voce
  if (pressPressed) {
    // Voce "jogging"
    if (menuIndex == 0) {
      showMenu       = false;      
      currentState   = SELECTING_VALUE;  
      currentNumber  = 0;      
      selectedArrow  = -1;
      stepIndex      = 0;
      

      disegnaSchermoCompleto();
      delay(200);
    }
    // *** NUOVO: voce "positioning" ***
    else if (menuIndex == 1) {
      showMenu       = false;      
      positioningMode= true;          // Attiviamo la modalità "positioning"
      newPosition    = currentPosition; 
      stepIndex      = 0;             // Riparti dallo step più basso o come preferisci

      drawPositioningScreen();
      delay(200);
    }
    else {
      // Per le altre voci...
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Funzione non ancora implementata!");
      delay(1500);

      // Torna al menu
      drawMenu();
    }
  }
}

// ---------------------------------------------------------------------------
// DISEGNO SEMICERCHIO “BACK”
// ---------------------------------------------------------------------------
void drawBackButtonIcon() {
  int r = 20;
  int cx = 20;
  int cy = 0;

  uint16_t arcColor = TFT_NAVY;
  tft.fillCircle(cx, cy, r, arcColor);
  tft.fillRect(cx - r, cy - r, 2*r, r, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);

  int textY = cy + r/2 + 4;  
  tft.drawString("Back", cx, textY);
}

// ---------------------------------------------------------------------------
// DISEGNO SCHERMO COMPLETO (STATO JOGGING)
// ---------------------------------------------------------------------------
void disegnaSchermoCompleto() {
  tft.fillScreen(TFT_BLACK);

  // Mostriamo in alto a sinistra il tasto "Back"
  drawBackButtonIcon();

  uint16_t valueColor = (currentState == SELECTING_VALUE) ? selectedColor : TFT_DARKGREY;

  // 1) Testo "Position" e relativo valore
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("Position", 120, 70);
  tft.drawString(String(currentPosition), 280, 70);

  // 2) Testo "Move" e currentNumber
  tft.setTextSize(4);
  tft.setTextColor(valueColor, TFT_BLACK);
  tft.drawString("Move", 100, 120);
  tft.drawString(String(currentNumber),   280, 120);

  // 3) Testo "Step"
  tft.setTextSize(2);
  tft.setTextColor(valueColor, TFT_BLACK);
  tft.drawString("(Step: " + String(steps[stepIndex]) + ")", 160, 170);

  // 4) Frecce (sinistra/destra) in basso
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
  
  tft.setTextColor(arrowLeftColor, TFT_BLACK);
  tft.drawString("<---", 100, yFrecce);
  tft.setTextColor(arrowRightColor, TFT_BLACK);
  tft.drawString("--->", 220, yFrecce);
}

// ---------------------------------------------------------------------------
// FUNZIONE AUSILIARIA: Controlla se è premuto il pulsante A (back) in Jogging
// ---------------------------------------------------------------------------
void checkBackButton() {
  if (digitalRead(WIO_KEY_C) == LOW) {
    // L'utente vuole tornare al menu
    showMenu = true;
    drawMenu();
    // Piccolo delay per evitare rimbalzi
    delay(300);
  }
}

// *** NUOVO: funzione ausiliaria per "back" in positioning ***
void checkBackButtonPositioning() {
  if (digitalRead(WIO_KEY_C) == LOW) {
    // Disattiviamo la modalità positioning e torniamo al menu
    positioningMode = false;
    showMenu = true;
    drawMenu();
    delay(300);
  }
}

// ---------------------------------------------------------------------------
// AGGIORNAMENTO STATO SELECTING_VALUE (JOGGING)
// ---------------------------------------------------------------------------
void updateSelectingValue() {
  checkBackButton();  // Controlla se l’utente ha premuto “Back”
  if (showMenu) return;

  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // Aumenta spostamento
  if (rightPressed) {
    currentNumber += steps[stepIndex];
    disegnaSchermoCompleto();
    delay(200);
  }

  // Diminuisci spostamento
  if (leftPressed && currentNumber > 0) {
    currentNumber -= steps[stepIndex];
    if (currentNumber < 0) currentNumber = 0;
    disegnaSchermoCompleto();
    delay(200);
  }

  // Step successivo (UP)
  if (upPressed) {
    if (stepIndex < numSteps - 1) {
      stepIndex++;
    }
    disegnaSchermoCompleto();
    delay(200);
  }

  // Step precedente (DOWN)
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
    selectedArrow = -1;
    disegnaSchermoCompleto();
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// AGGIORNAMENTO STATO SELECTING_DIRECTION (JOGGING)
// ---------------------------------------------------------------------------
void updateSelectingDirection() {
  checkBackButton();  
  if (showMenu) return;

  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // Se premi UP, torni a SELECTING_VALUE
  if (upPressed) {
    currentState = SELECTING_VALUE;
    disegnaSchermoCompleto();
    delay(200);
    return;
  }

  // Freccia sinistra
  if (leftPressed) {
    selectedArrow = 0;
    int newPos = currentPosition - currentNumber;
    if (newPos < minPosition) {
      newPos = minPosition;
    }
    currentPosition = newPos;

    // Invio comandi G-code
    Serial3.println("G90");     
    Serial3.print("G0 X");
    Serial3.println(currentPosition);

    disegnaSchermoCompleto();
    delay(200);
  }

  // Freccia destra
  if (rightPressed) {
    selectedArrow = 1;
    int newPos = currentPosition + currentNumber;
    if (newPos > maxPosition) {
      newPos = maxPosition;
    }
    currentPosition = newPos;

    // Invio comandi G-code
    Serial3.println("G90");     
    Serial3.print("G0 X");
    Serial3.println(currentPosition);

    disegnaSchermoCompleto();
    delay(200);
  }

  // Se premi il tasto centrale (PRESS), puoi decidere cosa fare
  if (pressPressed) {
    // Esempio: potresti andare in FINISHED, oppure non fare nulla
    // currentState = FINISHED; 
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// STATO: FINISHED (se servisse, in jogging)
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

  tft.setCursor(10, 200);
  tft.print("Posizione: ");
  tft.println(currentPosition);
}

// ---------------------------------------------------------------------------
// *** NUOVE FUNZIONI PER LA MODALITA' "positioning" ***
// ---------------------------------------------------------------------------

// Disegno della schermata positioning
void drawPositioningScreen() {
  tft.fillScreen(TFT_BLACK);

  // 1) Disegno pulsante Back (semicerchio)
  drawBackButtonIcon();

  // 2) Titolo
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("POSITIONING", 160, 40);

  // 3) Mostra la posizione corrente
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Current:", 100, 90);
  tft.drawString(String(currentPosition), 270, 90);

  // 4) Mostra la nuova posizione
  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("New:", 100, 130);
  tft.drawString(String(newPosition), 270, 130);

  // 5) Mostra lo Step
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("(Step: " + String(steps[stepIndex]) + ")", 160, 170);

 
}

// Gestione input in positioning
void updatePositioning() {
  // Controllo se premiamo "Back"
  checkBackButtonPositioning();
  if (showMenu) return;

  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // UP: aumenta indice step
  if (upPressed) {
    if (stepIndex < numSteps - 1) {
      stepIndex++;
    }
    drawPositioningScreen();
    delay(200);
  }

  // DOWN: diminuisce indice step
  if (downPressed) {
    if (stepIndex > 0) {
      stepIndex--;
    }
    drawPositioningScreen();
    delay(200);
  }

  // RIGHT: newPosition += step
  if (rightPressed) {
    newPosition += steps[stepIndex];
    if (newPosition > maxPosition) newPosition = maxPosition;
    drawPositioningScreen();
    delay(200);
  }

  // LEFT: newPosition -= step
  if (leftPressed) {
    newPosition -= steps[stepIndex];
    if (newPosition < minPosition) newPosition = minPosition;
    drawPositioningScreen();
    delay(200);
  }

  // PRESS: invio nuova posizione via seriale, aggiorno currentPosition
  if (pressPressed) {
    Serial3.println("G90");
    Serial3.print("G0 X");
    Serial3.println(newPosition);

    // Aggiorna la "currentPosition"
    currentPosition = newPosition;

    // Ridisegno per vedere subito la current allineata con la new
    drawPositioningScreen();
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial3.begin(115200);
  pinPeripheral(D0, PIO_SERCOM_ALT);
  pinPeripheral(D1, PIO_SERCOM_ALT);
  delay(1000);

  // Inizializza display
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Pin per i pulsanti
  pinMode(WIO_KEY_A,   INPUT_PULLUP); 
  pinMode(WIO_KEY_B,   INPUT_PULLUP);
  pinMode(WIO_KEY_C,   INPUT_PULLUP); // <-- BACK

  pinMode(WIO_5S_UP,    INPUT_PULLUP);
  pinMode(WIO_5S_DOWN,  INPUT_PULLUP);
  pinMode(WIO_5S_LEFT,  INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  drawMenu();
}

// ---------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------
void loop() {
  if (showMenu) {
    // Aggiorno il menu se è visibile
    updateMenu();
  }
  else {
    // Se siamo in modalità "positioning"
    if (positioningMode) {
      updatePositioning();
    }
    // Altrimenti, siamo in modalità "jogging"
    else {
      switch (currentState) {
        case SELECTING_VALUE:
          updateSelectingValue();
          break;

        case SELECTING_DIRECTION:
          updateSelectingDirection();
          break;

        case FINISHED:
          mostraSchermataFinale();
          while(true) { delay(100); }
          // Oppure: showMenu = true; drawMenu(); break;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// HANDLER PER LA SERIALE (necessario su Wio Terminal)
// ---------------------------------------------------------------------------
void SERCOM4_0_Handler() { Serial3.IrqHandler(); }
void SERCOM4_1_Handler() { Serial3.IrqHandler(); }
void SERCOM4_2_Handler() { Serial3.IrqHandler(); }
void SERCOM4_3_Handler() { Serial3.IrqHandler(); }
