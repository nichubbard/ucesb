#define EXPERIMENT_NAME "G-22-00115"

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x200 , "Trav. MUSIC" },
  { 0x300 , "MR-TOF" },
  { 0x400 , "DESPEC-DEGAS" },
  { 0x500 , "DESPEC-bPlas" },
  { 0x700 , "DESPEC-AIDA" },
  { 0xa00 , "EXPERT #1" },
  { 0x1100 , "EXPERT #2" },
  { 0x1300 , "EXPERT #3" },
  { 0x1400 , "EXPERT #4" },
  { 0x1500 , "EXPERT #5" },
  { 0x1600 , "EXPERT #6" },
  { 0x1700 , "EXPERT #7" },
  { 0x1900 , "EXPERT #8" },
};

std::vector<int> expected = { 0x100, 0x200, 0x400, 0x500, 0x700 };

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 1);
static constexpr size_t SCALER_START_EXTR = 8;
static constexpr size_t SCALER_STOP_EXTR = 9;

