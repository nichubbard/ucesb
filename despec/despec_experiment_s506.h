#define EXPERIMENT_NAME "S506"

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x400 , "HPGe" },
  { 0x500 , "bPlas" },
};

std::vector<int> expected = { 0x100, 0x400, 0x500 };

std::vector<std::string> scalers =
{
};

std::vector<int> scaler_order =
{
};

static constexpr size_t AIDA_DSSDS = 0;
static constexpr size_t AIDA_FEES = 0;

std::map<int, int> aida_dssd_map =
{
};

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_FATIMA_COUNT = 0;
static constexpr size_t SCALER_FRS_FRS_COUNT = 32;
static constexpr size_t SCALER_FRS_MAIN_COUNT = 32;
static constexpr size_t SCALER_COUNT = SCALER_FATIMA_COUNT + SCALER_FRS_FRS_COUNT + SCALER_FRS_MAIN_COUNT;

static constexpr int FRS_TPAT_PULSER = (1 << 8);
static constexpr size_t SCALER_START_EXTR = SCALER_FATIMA_COUNT + 8;
static constexpr size_t SCALER_STOP_EXTR = SCALER_FATIMA_COUNT + 9;

