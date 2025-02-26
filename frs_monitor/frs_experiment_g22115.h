#define EXPERIMENT_NAME "G-22-00115"

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x200 , "Trav. MUSIC" },
  { 0x300 , "MR-TOF" },
  { 0x400 , "DESPEC-DEGAS" },
  { 0x500 , "DESPEC-bPlas" },
  { 0x700 , "DESPEC-AIDA" },
};

std::vector<int> expected = { 0x100, 0x200, 0x400, 0x500, 0x700 };

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 1);
static constexpr size_t SCALER_START_EXTR = 8;
static constexpr size_t SCALER_STOP_EXTR = 9;

