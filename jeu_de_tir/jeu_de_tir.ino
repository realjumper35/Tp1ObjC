// TP1 - Jeu de tir
// 420-555-AL - ESP8266 (D1 mini)

// ---------- Broches ----------
const int PIN_SER    = 14; // D5 - DS (595)
const int PIN_SRCLK  = 12; // D6 - SHCP (595)
const int PIN_RCLK   = 13; // D7 - STCP (595)
const int PIN_PRISE  = 5;  // D1
const int PIN_CIBLE1 = 4;  // D2
const int PIN_CIBLE2 = 0;  // D3 - DEL vers 3V3, active a LOW
const int PIN_CIBLE3 = 2;  // D4 - DEL vers 3V3, active a LOW
const int PIN_BOUTON = 16; // D0 - pull-up externe, appuye = LOW
const int PIN_BUZZER = 15; // D8 - via transistor S8050

// ---------- Parametres (ms) ----------
const unsigned long DUREE_ANIM_DEBUT    = 2000;
const unsigned long DUREE_PAUSE         = 1000;
const unsigned long DUREE_AFFICHE_CIBLE = 1000;
const unsigned long DELAI_NIVEAU        = 400;
const unsigned long DUREE_TIR           = 500;
const unsigned long DUREE_RESULTAT      = 1500;
const unsigned long DUREE_FIN           = 3500;
const unsigned long DUREE_ATTENTE_FIN   = 2000;
const unsigned long DELAI_ANTIREBOND    = 20;

const int NIVEAU_MAX      = 5;
const int POINTS_VICTOIRE = 3;
const int PRISES_DEFAITE  = 2;

// Types declares avant les fonctions (prototypes generes par l'IDE)
enum Etat { DEBUT_PARTIE, PAUSE, AFFICHE_CIBLE, ATTENTE_TIR, CHARGE, TIR, RESULTAT, FIN_PARTIE };
struct Note { int freq; int duree; }; 

// ---------- Sons ----------
const bool BUZZER_ACTIF = false;


const Note FANFARE_DEBUT[] = {
  {392,120},{0,25},{523,120},{0,25},{659,120},{0,25},{784,360},{0,60},
  {659,150},{0,25},{784,650}
};

const Note SON_VOL[] = {
  {1500,35},{1400,35},{1300,35},{1200,35},{1100,35},{1000,35},
  {900,35},{800,35},{700,35},{600,35},{500,40}
};

const Note SON_SUCCES[] = { {1319,80},{0,30},{1760,250} };

const Note SON_ECHEC[] = {
  {392,200},{0,40},{370,200},{0,40},{349,200},{0,40},
  {330,60},{311,60},{330,60},{311,60},{330,60},{311,60},{330,60},{311,60},{330,60},{311,60}
};


const Note FANFARE_VICTOIRE[] = {
  {784,100},{0,25},{784,100},{0,25},{784,200},{0,50},
  {784,100},{0,25},{784,100},{0,25},{784,200},{0,50},
  {784,100},{0,25},{784,100},{0,25},{1047,200},{0,25},{1175,200},{0,25},{1319,450}
};

const Note FANFARE_DEFAITE[] = {
  {466,390},{0,25},{466,290},{0,25},{466,100},{0,25},{466,390},{0,25},
  {554,290},{0,25},{523,100},{0,25},{523,290},{0,25},{466,100},{0,25},
  {466,290},{0,25},{440,100},{0,25},{466,600}
};

Note noteCharge[1];


const Note* melodie = nullptr;
int nbNotes = 0;
int indexNote = 0;
unsigned long debutNote = 0;

void demarrerNote() {
  int f = melodie[indexNote].freq;
  if (f == 0)            { noTone(PIN_BUZZER); digitalWrite(PIN_BUZZER, LOW); }
  else if (BUZZER_ACTIF) { digitalWrite(PIN_BUZZER, HIGH); }
  else                   { tone(PIN_BUZZER, f); }
  debutNote = millis();
}

void arreterSon() {
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  melodie = nullptr;
}

void jouer(const Note* m, int n) {
  melodie = m;
  nbNotes = n;
  indexNote = 0;
  demarrerNote();
}

#define JOUER(m) jouer(m, sizeof(m) / sizeof((m)[0]))

void majSon() {
  if (melodie == nullptr) return;
  if (millis() - debutNote >= (unsigned long)melodie[indexNote].duree) {
    indexNote++;
    if (indexNote >= nbNotes) arreterSon();
    else demarrerNote();
  }
}

Etat etat = DEBUT_PARTIE;
unsigned long debutEtat = 0;

int cible        = 0; 
int niveau       = 0; 
int cibleTouchee = 0; 
bool succes      = false;
int score        = 0;
int prises       = 0;

void changerEtat(Etat nouvelEtat) {
  etat = nouvelEtat;
  debutEtat = millis();
  Serial.printf("Etat -> %d | score %d | prises %d\n", etat, score, prises);
}


void ecrire595(uint8_t octet) {
  shiftOut(PIN_SER, PIN_SRCLK, MSBFIRST, octet);
  digitalWrite(PIN_RCLK, HIGH); // latch
  digitalWrite(PIN_RCLK, LOW);
}

uint8_t octetBarre(int n) { return (1 << n) - 1; }       // Q0-Q4
uint8_t octetScore(int s) { return ((1 << s) - 1) << 5; } // Q5-Q7

void afficher595(int niv, int sc) {
  ecrire595(octetBarre(niv) | octetScore(sc));
}

void allumerCible(int n) { 
  digitalWrite(PIN_CIBLE1, n == 1 ? HIGH : LOW);
  digitalWrite(PIN_CIBLE2, n == 2 ? LOW : HIGH);
  digitalWrite(PIN_CIBLE3, n == 3 ? LOW : HIGH);
}

void allumerToutesCibles(bool allume) {
  digitalWrite(PIN_CIBLE1, allume ? HIGH : LOW);
  digitalWrite(PIN_CIBLE2, allume ? LOW : HIGH);
  digitalWrite(PIN_CIBLE3, allume ? LOW : HIGH);
}

void allumerPrise(bool allume) { digitalWrite(PIN_PRISE, allume ? HIGH : LOW); }

void toutEteindre() {
  ecrire595(0);
  allumerCible(0);
  allumerPrise(false);
}

// ---------- Bouton ----------
bool boutonAppuye = false;
bool dernierBrut  = false;
unsigned long dernierChangement = 0;
bool evtAppui   = false;
bool evtRelache = false;

void lireBouton() {
  evtAppui = false;
  evtRelache = false;
  bool brut = (digitalRead(PIN_BOUTON) == LOW);
  if (brut != dernierBrut) {
    dernierBrut = brut;
    dernierChangement = millis();
  }
  if (millis() - dernierChangement >= DELAI_ANTIREBOND && brut != boutonAppuye) {
    boutonAppuye = brut;
    if (boutonAppuye) evtAppui = true;
    else              evtRelache = true;
  }
}

// ---------- Regles ----------
int cibleAtteinte(int niv) {
  switch (niv) {
    case 2:  return 1;
    case 3:  return 2;
    case 4:  return 3;
    default: return 0;
  }
}

void demarrerRonde() {
  cible = random(1, 4);
  niveau = 0;
  changerEtat(AFFICHE_CIBLE);
}

void tirer(bool horsCircuit) {
  cibleTouchee = horsCircuit ? 0 : cibleAtteinte(niveau);
  succes = (!horsCircuit && cibleTouchee == cible);
  Serial.printf("Tir niveau %d -> cible %d (visee %d) : %s\n",
                niveau, cibleTouchee, cible, succes ? "SUCCES" : "ECHEC");
  changerEtat(TIR);
  JOUER(SON_VOL);
}

// ---------- Programme ----------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_SER, OUTPUT);
  pinMode(PIN_SRCLK, OUTPUT);
  pinMode(PIN_RCLK, OUTPUT);
  pinMode(PIN_PRISE, OUTPUT);
  pinMode(PIN_CIBLE1, OUTPUT);
  pinMode(PIN_CIBLE2, OUTPUT);
  pinMode(PIN_CIBLE3, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOUTON, INPUT);
  digitalWrite(PIN_BUZZER, LOW);
  toutEteindre();

  randomSeed(analogRead(A0));
  changerEtat(DEBUT_PARTIE);
  JOUER(FANFARE_DEBUT);
}

void loop() {
  lireBouton();
  majSon();
  unsigned long t = millis() - debutEtat;

  switch (etat) {

    case DEBUT_PARTIE: {
      bool groupeA = (t / 250) % 2 == 0;
      if (groupeA) { ecrire595(octetBarre(NIVEAU_MAX)); allumerToutesCibles(true);  allumerPrise(false); }
      else         { ecrire595(octetScore(3));          allumerToutesCibles(false); allumerPrise(true);  }
      if (t >= DUREE_ANIM_DEBUT) {
        toutEteindre();
        changerEtat(PAUSE);
      }
      break;
    }

    case PAUSE:
      if (t >= DUREE_PAUSE) demarrerRonde();
      break;

    case AFFICHE_CIBLE:
      allumerCible(cible);
      if (t >= DUREE_AFFICHE_CIBLE) {
        allumerCible(0);
        changerEtat(ATTENTE_TIR);
      }
      break;

    case ATTENTE_TIR:
      if (evtAppui) changerEtat(CHARGE);
      break;

    case CHARGE: {
      int nouveauNiveau = 1 + t / DELAI_NIVEAU;
      if (nouveauNiveau != niveau) {
        niveau = nouveauNiveau;
        noteCharge[0] = { 400 + niveau * 200, 80 };
        JOUER(noteCharge);
      }
      if (niveau >= NIVEAU_MAX) {
        niveau = NIVEAU_MAX;
        afficher595(niveau, score);
        tirer(true);
      } else if (evtRelache) {
        tirer(false);
      } else {
        afficher595(niveau, score);
      }
      break;
    }

    case TIR:
      if (t >= DUREE_TIR) {
        if (succes) score++;
        else        prises++;
        afficher595(0, score);
        changerEtat(RESULTAT);
        if (succes) JOUER(SON_SUCCES);
        else        JOUER(SON_ECHEC);
      }
      break;

    case RESULTAT: {
      allumerCible(cibleTouchee);
      bool clignote = (t > DUREE_RESULTAT - 500) || ((t / 150) % 2 == 0);
      if (succes) {
        afficher595(0, clignote ? score : score - 1);
        allumerPrise(prises > 0);
      } else {
        afficher595(0, score);
        allumerPrise(clignote);
      }
      if (t >= DUREE_RESULTAT) {
        allumerCible(0);
        if (score >= POINTS_VICTOIRE || prises >= PRISES_DEFAITE) {
          toutEteindre();
          changerEtat(FIN_PARTIE);
          if (score >= POINTS_VICTOIRE) JOUER(FANFARE_VICTOIRE);
          else                          JOUER(FANFARE_DEFAITE);
        } else {
          changerEtat(PAUSE);
        }
      }
      break;
    }

    case FIN_PARTIE: {
      bool victoire = (score >= POINTS_VICTOIRE);
      if (t < DUREE_FIN) {
        bool phase = (t / 200) % 2 == 0;
        if (victoire) ecrire595(phase ? 0b10100000 : 0b01000000);
        else          allumerPrise(phase);
      } else if (t < DUREE_FIN + DUREE_ATTENTE_FIN) {
        toutEteindre();
      } else {
        score = 0;
        prises = 0;
        changerEtat(DEBUT_PARTIE);
        JOUER(FANFARE_DEBUT);
      }
      break;
    }
  }
}
