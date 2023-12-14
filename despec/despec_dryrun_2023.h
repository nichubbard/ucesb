#define EXPERIMENT_NAME "Implantation Test 1"

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x500 , "bPlas" },
  { 0x700 , "AIDA" },
  { 0x1500, "Scalers" },
  { 0x1600, "FAT. TMX" },
};

std::vector<int> expected = { 0x100, 0x500, 0x700, 0x1500, 0x1600 };

std::vector<std::string> scalers =
{
  "bPlast Free",
  "bPlast Accepted",
  "",
  "DEGAS Free",
  "DEGAS Accepted",
  "BGO",
  "bPlast Up",
  "bPlast Down",
  "bPlast AND",
  "SCI41 L",
  "SCI41 R",
  "SCI21 L",
  "SCI21 R",
  "Pulser",
  "DTAS",
};

std::vector<int> scaler_order =
{
  0, 1,
  3, 4,
  5, -1,
  6, 7,
  8, -1,
  9, 10,
  11, 12,
  13, 14
};

static constexpr size_t AIDA_DSSDS = 1;
static constexpr size_t AIDA_FEES = 8;

std::map<int, int> aida_dssd_map =
{
  {1, 1},
  {2, 1},
  {3, 1},
  {4, 1},

  {5, 1},
  {6, 1},
  {7, 1},
  {8, 1},
};

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FATIMA_COUNT = 16;
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FATIMA_COUNT + SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 8);
static constexpr size_t SCALER_START_EXTR = SCALER_FATIMA_COUNT + 8;
static constexpr size_t SCALER_STOP_EXTR = SCALER_FATIMA_COUNT + 9;

