#define EXPERIMENT_NAME "S101"

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x400 , "DEGAS" },
  { 0x500 , "bPlas" },
  { 0x700 , "AIDA" },
  { 0x1500, "FAT. VME" },
  { 0x1600, "FAT. TMX" },
  { 0x1700, "Beam Mon" },
  { 0x1800, "BB7" },
  { 0x1900, "BGO" },
};

std::vector<int> expected = { 0x100, 0x400, 0x500, 0x700, 0x1700, 0x1800 };

std::vector<std::string> scalers =
{
  "bPlast Free",
  "bPlast Accepted",
  "",
  "DEGAS Free",
  "DEGAS Accepted",
  "FATIMA Free",
  "FATIMA Accepted",
  "",
  "",
  "FRS Accepted",
  "SC41 R",
  "SC21 R",
  "1 MHz",
  "bPlas Upstream",
  "bPlas Downstream",
};

std::vector<int> scaler_order =
{
  0, 1,
  3, 4,
  5, 6,
  9, 12,
  10, 11,
  13, 14
};

static constexpr size_t AIDA_DSSDS = 2;
static constexpr size_t AIDA_FEES = 16;

std::map<int, int> aida_dssd_map =
{
  {1, 1},
  {2, 1},
  {3, 1},
  {4, 1},
  {5, 1},
  {6, 2},
  {7, 2},
  {8, 2},
  {9, 1},
  {10, 2},
  {11, 2},
  {12, 1},
  {13, 2},
  {14, 2},
  {15, 1},
  {16, 2},
};

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FATIMA_COUNT = 16;
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FATIMA_COUNT + SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 1);
static constexpr size_t SCALER_START_EXTR = SCALER_FATIMA_COUNT + 8;
static constexpr size_t SCALER_STOP_EXTR = SCALER_FATIMA_COUNT + 9;

