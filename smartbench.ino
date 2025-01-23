#include <TFT_eSPI.h> // Libreria per il display
#include <SPI.h>
#include <wiring_private.h>

// Se il tuo ambiente supporta <vector>, includilo:
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <vector>

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

// -- Jogging
int  currentNumber    = 0;   
int  stepIndex        = 0;   
int  selectedArrow    = -1;  
int  currentPosition  = 0;   

enum JoggingState {
  SELECTING_VALUE,
  SELECTING_DIRECTION,
  FINISHED
};
JoggingState currentState = SELECTING_VALUE;

// -- Positioning
bool positioningMode = false;  // Flag per "positioning"
int  newPosition     = 0;      // Posizione target

// -- Joint
bool jointMode = false;   // Flag per "joint"

// Questi 4 campi corrispondono ai parametri del finger joint
int bladeValue  = 0;  // Blade
int startValue  = 0;  // Start
int dadoLarge   = 0;  // Dado Large (larghezza_incastro)
int totalLarge  = 0;  // Total Large (larghezza_pezzo)

// Indice per sapere quale campo stiamo modificando in Joint
// 0=Blade, 1=Start, 2=Dado Large, 3=Total Large, 4=Step, 5=START
int jointIndex = 0; 

// Vettore che conterrà la sequenza di tagli calcolati
std::vector<float> tagli_sequenza;

// ---------------------------------------------------------------------------
// FUNZIONI DI SUPPORTO
// ---------------------------------------------------------------------------

// 1) Attende pressione e rilascio del tasto centrale (bloccante)
void waitForCenterPress() {
  // Aspetta che il pulsante centrale sia premuto
  while (digitalRead(WIO_5S_PRESS) == HIGH) {
    delay(10);
  }
  // Ora è premuto, aspetta che venga rilasciato
  while (digitalRead(WIO_5S_PRESS) == LOW) {
    delay(10);
  }
  delay(200); // Debounce
}

// 2) Mostra messaggio "CUT X/totalCuts" e "Make cut and click for next cut"
void showCutMessage(int cutIndex, int totalCuts, int machinePos) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.setCursor(10, 30);
  // Esempio: "CUT 1/5" se cutIndex=0 e totalCuts=5
  String msg = "CUT " + String(cutIndex+1) + "/" + String(totalCuts) + " Pos: " + String(machinePos) ;
  tft.println(msg);

  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.println("Make cut and click for next cut");
}

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
      currentPosition= 0;

      disegnaSchermoCompleto();
      delay(200);
    }
    // Voce "positioning"
    else if (menuIndex == 1) {
      showMenu       = false;      
      positioningMode= true;          
      newPosition    = currentPosition; 
      stepIndex      = 0;             

      drawPositioningScreen();
      delay(200);
    }
    // Voce "joint"
    else if (menuIndex == 2) {
      showMenu   = false;
      jointMode  = true; 
      jointIndex = 0;  // Selezioniamo il primo campo (Blade)
      
      drawJointScreen();
      delay(200);
    }
    else {
      // Per la voce "settings" (non implementata)...
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
    showMenu = true;
    drawMenu();
    delay(300);
  }
}

// ---------------------------------------------------------------------------
// AGGIORNAMENTO STATO SELECTING_VALUE (JOGGING)
// ---------------------------------------------------------------------------
void updateSelectingValue() {
  checkBackButton();  
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

  // Se premi il tasto centrale (PRESS)
  if (pressPressed) {
    // Esempio: potresti andare in FINISHED
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
// *** POSITIONING ***
// ---------------------------------------------------------------------------
void checkBackButtonPositioning() {
  if (digitalRead(WIO_KEY_C) == LOW) {
    positioningMode = false;
    showMenu = true;
    drawMenu();
    delay(300);
  }
}

void drawPositioningScreen() {
  tft.fillScreen(TFT_BLACK);

  drawBackButtonIcon();

  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("POSITIONING", 160, 40);

  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Current:", 100, 90);
  tft.drawString(String(currentPosition), 270, 90);

  tft.setTextSize(3);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("New:", 100, 130);
  tft.drawString(String(newPosition), 270, 130);

  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("(Step: " + String(steps[stepIndex]) + ")", 160, 170);

  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int yFrecce = 220;
  tft.drawString("<---", 100, yFrecce);
  tft.drawString("--->", 220, yFrecce);
}

void updatePositioning() {
  checkBackButtonPositioning();
  if (showMenu) return;

  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  if (upPressed) {
    if (stepIndex < numSteps - 1) {
      stepIndex++;
    }
    drawPositioningScreen();
    delay(200);
  }

  if (downPressed) {
    if (stepIndex > 0) {
      stepIndex--;
    }
    drawPositioningScreen();
    delay(200);
  }

  if (rightPressed) {
    newPosition += steps[stepIndex];
    if (newPosition > maxPosition) newPosition = maxPosition;
    drawPositioningScreen();
    delay(200);
  }

  if (leftPressed) {
    newPosition -= steps[stepIndex];
    if (newPosition < minPosition) newPosition = minPosition;
    drawPositioningScreen();
    delay(200);
  }

  if (pressPressed) {
    Serial3.println("G90");
    Serial3.print("G0 X");
    Serial3.println(newPosition);

    currentPosition = newPosition;

    drawPositioningScreen();
    delay(200);
  }
}

// ---------------------------------------------------------------------------
// *** JOINT ***
// ---------------------------------------------------------------------------

void checkBackButtonJoint() {
  if (digitalRead(WIO_KEY_C) == LOW) {
    jointMode = false;
    showMenu  = true;
    drawMenu();
    delay(300);
  }
}

// Calcolo della sequenza di tagli (Finger joint)
void calcolaSequenzaFingerJoint() {
  // Svuotiamo il vettore esistente
  tagli_sequenza.clear();

  // Convertiamo i valori int in float
  float larghezza_dente    = (float)bladeValue;   // Blade
  float larghezza_incastro = (float)dadoLarge;    // Dado Large
  float inizio_taglio      = (float)startValue;   // Start
  float larghezza_pezzo    = (float)totalLarge;   // Total Large

  float posizione_attuale = inizio_taglio + larghezza_dente;

  while (posizione_attuale < larghezza_pezzo) {
    float fine_canale = posizione_attuale + larghezza_incastro - larghezza_dente;
    tagli_sequenza.push_back(fine_canale);

    while (posizione_attuale < fine_canale && posizione_attuale < larghezza_pezzo) {
      tagli_sequenza.push_back(posizione_attuale);
      posizione_attuale += larghezza_dente;
    }

    posizione_attuale = fine_canale + larghezza_incastro + larghezza_dente;
    if (posizione_attuale >= larghezza_pezzo) {
      break;
    }
  }
}

// *** NUOVA FUNZIONE DOJOINTSTART CHE ASPETTA LA PRESSIONE TRA I TAGLI ***
void doJointStart() {
  delay(1000);
  // 1) Calcoliamo la sequenza di tagli
  int  currentPosition  = 0;
  calcolaSequenzaFingerJoint();

  // 2) Se la lista è vuota, mostriamo un messaggio e torniamo
  if (tagli_sequenza.empty()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 50);
    tft.println("No cuts calculated!");
    delay(2000);

    // Torniamo alla schermata joint
    drawJointScreen();
    return;
  }

  // 3) Procedura iterativa
  size_t totalCuts = tagli_sequenza.size();
  for (size_t i = 0; i < totalCuts; i++) {
    float pos = tagli_sequenza[i];

    // Invia G-code per la posizione
    Serial3.println("G90");
    Serial3.print("G0 X");
    Serial3.println(pos);

    // Mostra messaggio di attesa utente
    showCutMessage(i, totalCuts, pos);

    // Aspetta che l’utente prema il tasto centrale
    waitForCenterPress();
  }

  // 4) Fine tagli
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 50);
  tft.println("END of cuts");

  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println("Click to continue...");

  waitForCenterPress();

  // Torna alla schermata di input joint
  drawJointScreen();
}

// Disegno schermata Joint
void drawJointScreen() {
  tft.fillScreen(TFT_BLACK);
  drawBackButtonIcon(); // Back in alto a sinistra

  int lineSpacing = 30;
  int startY = 60;

  // Campi: Blade, Start, Dado Large, Total Large, Step, e infine START
  for (int i = 0; i < 6; i++) {
    int yPos = startY + i * lineSpacing;
    tft.setCursor(10, yPos);

    uint16_t color = (i == jointIndex) ? selectedColor : TFT_WHITE;
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(3);

    switch(i) {
      case 0: {
        // Blade
        String s = "Blade: " + String(bladeValue);
        tft.println(s);
        break;
      }
      case 1: {
        // Start
        String s = "Start: " + String(startValue);
        tft.println(s);
        break;
      }
      case 2: {
        // Dado Large
        String s = "Dado Large: " + String(dadoLarge);
        tft.println(s);
        break;
      }
      case 3: {
        // Total Large
        String s = "Total Large: " + String(totalLarge);
        tft.println(s);
        break;
      }
      case 4: {
        // Step
        String s = "Step: " + String(steps[stepIndex]);
        tft.println(s);
        break;
      }
      case 5: {
        // START (pulsante "virtuale")
        tft.println("     START");
        break;
      }
    }
  }
}

// Gestione input in Joint
void updateJoint() {
  checkBackButtonJoint();
  if (showMenu) return;

  bool upPressed     = (digitalRead(WIO_5S_UP)    == LOW);
  bool downPressed   = (digitalRead(WIO_5S_DOWN)  == LOW);
  bool leftPressed   = (digitalRead(WIO_5S_LEFT)  == LOW);
  bool rightPressed  = (digitalRead(WIO_5S_RIGHT) == LOW);
  bool pressPressed  = (digitalRead(WIO_5S_PRESS) == LOW);

  // Navigazione su/giù tra i campi (0..5)
  if (upPressed) {
    if (jointIndex > 0) {
      jointIndex--;
      drawJointScreen();
      delay(200);
    }
  }
  if (downPressed) {
    if (jointIndex < 5) {
      jointIndex++;
      drawJointScreen();
      delay(200);
    }
  }

  // Se stiamo selezionando una delle voci: 
  // 0=Blade, 1=Start, 2=Dado Large, 3=Total Large, 4=Step, 5=START
  if (leftPressed || rightPressed) {
    if (jointIndex == 0) {
      // Blade
      if (rightPressed) {
        bladeValue += steps[stepIndex];
      } else if (leftPressed) {
        bladeValue -= steps[stepIndex];
        if (bladeValue < 0) bladeValue = 0;
      }
    }
    else if (jointIndex == 1) {
      // Start
      if (rightPressed) {
        startValue += steps[stepIndex];
      } else if (leftPressed) {
        startValue -= steps[stepIndex];
        if (startValue < 0) startValue = 0;
      }
    }
    else if (jointIndex == 2) {
      // Dado Large
      if (rightPressed) {
        dadoLarge += steps[stepIndex];
      } else if (leftPressed) {
        dadoLarge -= steps[stepIndex];
        if (dadoLarge < 0) dadoLarge = 0;
      }
    }
    else if (jointIndex == 3) {
      // Total Large
      if (rightPressed) {
        totalLarge += steps[stepIndex];
      } else if (leftPressed) {
        totalLarge -= steps[stepIndex];
        if (totalLarge < 0) totalLarge = 0;
      }
    }
    else if (jointIndex == 4) {
      // Step (qui modifichiamo stepIndex)
      if (rightPressed) {
        if (stepIndex < numSteps - 1) {
          stepIndex++;
        }
      } else if (leftPressed) {
        if (stepIndex > 0) {
          stepIndex--;
        }
      }
    }
    // jointIndex == 5 (START) → left/right non fa nulla

    drawJointScreen();
    delay(200);
  }

  // Se premi il tasto centrale
  if (pressPressed) {
    // Se siamo su "START" (jointIndex == 5), lanciamo la funzione
    if (jointIndex == 5) {
      doJointStart();
    }
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
    updateMenu();
  }
  else {
    if (positioningMode) {
      updatePositioning();
    }
    else if (jointMode) {
      updateJoint();
    }
    else {
      // Modalità Jogging
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
