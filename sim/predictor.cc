#include "predictor.h"

/////////////// STORAGE BUDGET JUSTIFICATION ////////////////

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

// Constructeur du prédicteur
PREDICTOR::PREDICTOR(char *prog, int argc, char *argv[])
{
   // La trace est tjs présente, et les arguments sont ceux que l'on désire
   if (argc != 3) {
      fprintf(stderr, "usage: %s <trace> pcbits countbits type(0:default, 1:gshare, 2:local, 3:combined)\n", prog);
      exit(-1);
   }

   uint32_t pcbits    = strtoul(argv[0], NULL, 0);
   uint32_t countbits = strtoul(argv[1], NULL, 0);
   type      = strtoul(argv[2], NULL, 0); // 0: default, 1: gshare, 2: local, 3: combined

   nentries = (1 << pcbits);        // nombre d'entrées dans la table
   pcmask   = (nentries - 1);       // masque pour n'accéder qu'aux bits significatifs de PC
   countmax = (1 << countbits) - 1; // valeur max atteinte par le compteur à saturation
   table    = new uint32_t[nentries]();

   GHR = 0;
   LHT = new uint32_t[1024]();
   CPT_Comb = new uint32_t[nentries]();
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool PREDICTOR::GetPrediction(UINT64 PC)
{
   uint32_t v = 0;
   if (type == 0) {
      // Simple predictor
      v = table[PC & pcmask];
   }
   if (type == 1) {
      // Gshare 
      v = table[((PC >> 2) ^ GHR) & pcmask];
   }
   if (type == 2) {
      // Local Predictor
      uint32_t local_history = LHT[PC & 1023]; // On récupère l'historique de ce PC
      v = table[local_history & pcmask];
   }
   if (type == 3) {
      // Combined Predictor
      uint32_t local_history = LHT[PC & 1023];
      uint32_t v_gshare = table[((PC >> 2) ^ GHR) & pcmask];
      uint32_t v_local = table[local_history & pcmask];

      // Choix du prédicteur via le Meta-Predictor
      uint32_t choice = CPT_Comb[PC & pcmask];
      if (choice > (countmax / 2)) {
         v = v_local; // On choisit Local
      } else {
         v = v_gshare;  // On choisit Gshare
      }
   }

   return v > (countmax / 2) ? TAKEN : NOT_TAKEN;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void PREDICTOR::UpdatePredictor(UINT64 PC, OpType opType, bool resolveDir, bool predDir, UINT64 branchTarget)
{
   uint32_t v = 0;
   if (type == 0) {
      // Simple predictor
      uint32_t v = table[PC & pcmask];
      table[PC & pcmask] = resolveDir == TAKEN ? SatIncrement(v, countmax) : SatDecrement(v);
   }
   else if (type == 1) {
      // Gshare 
      v = table[((PC >> 2) ^ GHR) & pcmask];
      table[((PC >> 2) ^ GHR) & pcmask] = (resolveDir == TAKEN) ? SatIncrement(v, countmax) : SatDecrement(v);
      GHR = ((GHR << 1) | (resolveDir == TAKEN ? 1 : 0)) & pcmask;
   }
   else if (type == 2) {
      // Local Predictor
      uint32_t local_history = LHT[PC & 1023];
      v = table[local_history & pcmask];
      table[local_history & pcmask] = (resolveDir == TAKEN) ? SatIncrement(v, countmax) : SatDecrement(v);
      // Mise à jour de l'historique local
      LHT[PC & 1023] = ((local_history << 1) | (resolveDir == TAKEN ? 1 : 0)) & pcmask;
   }
   else if (type == 3) {
      // Combined Predictor
      uint32_t local_history = LHT[PC & 1023];
      uint32_t v_gshare = table[((PC >> 2) ^ GHR) & pcmask];
      uint32_t v_local = table[local_history & pcmask];
      // Mise à jour du Meta-Predictor
      uint32_t choice = CPT_Comb[PC & pcmask];
      bool pred_gshare = v_gshare > (countmax / 2);
      bool pred_local = v_local > (countmax / 2);

      if (pred_gshare != pred_local) {
         if (pred_local == (resolveDir == TAKEN)) {
            CPT_Comb[PC & pcmask] = SatIncrement(choice, countmax);
         } else {
            CPT_Comb[PC & pcmask] = SatDecrement(choice);
         }
      }
      // Mise à jour des deux prédicteurs
      table[((PC >> 2) ^ GHR) & pcmask] = (resolveDir == TAKEN) ? SatIncrement(v_gshare, countmax) : SatDecrement(v_gshare);
      table[local_history & pcmask] = (resolveDir == TAKEN) ? SatIncrement(v_local, countmax) : SatDecrement(v_local);

      // Mise à jour LHT et GHR
      LHT[PC & 1023] = ((local_history << 1) | (resolveDir == TAKEN ? 1 : 0)) & pcmask;
      GHR = ((GHR << 1) | (resolveDir == TAKEN ? 1 : 0)) & pcmask;
   }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void PREDICTOR::TrackOtherInst(UINT64 PC, OpType opType, bool branchDir, UINT64 branchTarget)
{
   // This function is called for instructions which are not
   // conditional branches, just in case someone decides to design
   // a predictor that uses information from such instructions.
   // We expect most contestants to leave this function untouched.
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////


/***********************************************************/
