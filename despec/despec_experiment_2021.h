
std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x400 , "HPGe" },
  { 0x500 , "bPlas" },
  { 0x700 , "AIDA" },
  { 0x1200, "FINGER" },
  { 0x1500, "FAT. VME" },
  { 0x1600, "FAT. TMX" },
};

std::vector<int> expected = { 0x100, 0x400, 0x500, 0x700, 0x1500, 0x1600 };

std::vector<std::string> scalers =
{
  "bPlast Free",
  "bPlast Accepted",
  "FATIMA TAMEX Free",
  "FATIMA TAMEX Accepted",
  "FATIMA VME Free",
  "FATIMA VME Accepted",
  "Ge Free",
  "Ge Accepted",
  "bPlast Up",
  "bPlast Down",
  "bPlast AND",
  "SCI41 L",
  "SCI41 R",
  "SCI42 L",
  "SCI42 R"
};

std::vector<int> scaler_order =
{
  0, 1,
  2, 3,
  4, 5,
  6, 7,
  8, 9,
  10, -1,
  11, 12,
  13, 14
};

static constexpr size_t AIDA_DSSDS = 2;

std::map<int, int> aida_dssd_map =
{
  {1, 1},
  {2, 1},
  {3, 1},
  {4, 1},

  {5, 2},
  {6, 2},
  {7, 2},
  {8, 2},

  {9, 1},
  {10, 1},
  {11, 1},
  {12, 1},

  {13, 2},
  {14, 2},
  {15, 2},
  {16, 2}
};

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FATIMA_COUNT = 16;
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FATIMA_COUNT + SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 8);
static constexpr size_t SCALER_START_EXTR = SCALER_FATIMA_COUNT + 8;
static constexpr size_t SCALER_STOP_EXTR = SCALER_FATIMA_COUNT + 9;

