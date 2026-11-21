// Pruebas del núcleo de WRAM: configuración, catálogo y formato.
// Ejecutable autónomo: devuelve 0 si todo pasa. Con --junit <archivo> escribe un reporte JUnit.
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Format.h"
#include "core/MetricCatalog.h"

using namespace wram;

namespace {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

std::string g_failure;

#define TEST(name)                                       \
    static void name();                                  \
    static Registrar reg_##name(#name, name);            \
    static void name()

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond) && g_failure.empty()) {                                          \
            g_failure = std::string(__FILE__ ":") + std::to_string(__LINE__) + " " #cond; \
        }                                                                            \
    } while (0)

}  // namespace

TEST(defaults_have_six_unique_slots) {
    const Config c = Config::Defaults();
    CHECK(c.slots.size() == 6);
    CHECK(c.ActiveMask() != 0);
    CHECK(c.rows == 2);
}

TEST(serialize_parse_roundtrip) {
    Config c = Config::Defaults();
    c.rows = 1;
    c.fontSize = 14;
    c.font = L"Consolas";
    c.valueColor = 0xFFB000;
    c.offset = 40;
    c.theme = 2;
    c.slots[0].label = L"PROC";
    c.slots[0].threshold = 75;
    const Config back = ConfigCodec::Parse(ConfigCodec::Serialize(c));
    CHECK(back.rows == 1);
    CHECK(back.fontSize == 14);
    CHECK(back.font == L"Consolas");
    CHECK(back.valueColor == 0xFFB000);
    CHECK(back.offset == 40);
    CHECK(back.theme == 2);
    CHECK(back.slots.size() == c.slots.size());
    CHECK(back.slots[0].label == L"PROC");
    CHECK(back.slots[0].threshold == 75);
    CHECK(ConfigCodec::Serialize(back) == ConfigCodec::Serialize(c));
}

TEST(parse_clamps_out_of_range_values) {
    const Config c = ConfigCodec::Parse("[general]\ninterval=10\nrows=7\nfontSize=200\noffset=-5\n");
    CHECK(c.intervalMs == Config::kMinInterval);
    CHECK(c.rows == 2);
    CHECK(c.fontSize == Config::kMaxFontSize);
    CHECK(c.offset == 0);
}

TEST(parse_rejects_unknown_font_and_bad_colors) {
    const Config c = ConfigCodec::Parse("[general]\nfont=Comic Sans MS\nvalueColor=#zzzzzz\nalertColor=auto\n");
    CHECK(c.font == L"Segoe UI Variable Text");
    CHECK(c.valueColor == Config::kAutoColor);
    CHECK(c.alertColor == 0xFF5A4E);
}

TEST(parse_drops_duplicates_unknown_keys_and_caps_at_ten) {
    std::string ini = "[slots]\n1=cpu\n2=cpu\n3=nope|X|1|1\n";
    const char* keys[] = {"ram", "gpu", "gputemp", "cputemp", "cpuclock", "ramused", "vram", "disk", "netdown", "netup", "battery"};
    int n = 4;
    for (const char* k : keys) ini += std::to_string(n++) + "=" + k + "\n";
    const Config c = ConfigCodec::Parse(ini);
    CHECK(c.slots.size() == kMaxSlots);
    CHECK(c.slots[0].id == MetricId::CpuLoad);
    CHECK(c.slots[1].id == MetricId::RamLoad);
}

TEST(parse_empty_slots_section_means_no_metrics) {
    const Config c = ConfigCodec::Parse("[general]\nrows=1\n[slots]\n");
    CHECK(c.slots.empty());
}

TEST(labels_are_sanitized_and_truncated) {
    const Config c = ConfigCodec::Parse("[slots]\n1=cpu|AB=C[D]EFGHIJKLMN|50|1\n");
    CHECK(c.slots.size() == 1);
    CHECK(c.slots[0].label == L"ABCDEFGH");
}

TEST(threshold_is_clamped_to_metric_maximum) {
    const Config c = ConfigCodec::Parse("[slots]\n1=cpu|CPU|900|1\n2=uptime|ON|50|1\n");
    CHECK(c.slots[0].threshold == 100);
    CHECK(!c.slots[1].alert);  // el tiempo encendido no admite aviso
}

TEST(oversized_input_falls_back_to_defaults) {
    const std::string huge(70 * 1024, 'x');
    const Config c = ConfigCodec::Parse(huge);
    CHECK(c.slots.size() == Config::Defaults().slots.size());
}

TEST(alert_above_and_below) {
    CHECK(IsAlert(Info(MetricId::CpuLoad), 95, 90));
    CHECK(!IsAlert(Info(MetricId::CpuLoad), 50, 90));
    CHECK(IsAlert(Info(MetricId::Battery), 15, 20));
    CHECK(!IsAlert(Info(MetricId::Battery), 80, 20));
    CHECK(IsAlert(Info(MetricId::NetDown), 6.0 * 1024 * 1024, 5));  // umbral en MB/s
    CHECK(!IsAlert(Info(MetricId::CpuLoad), 100, 0));
}

TEST(catalog_keys_are_unique_and_resolvable) {
    for (size_t i = 0; i < kMetricCount; ++i) {
        const MetricInfo& info = Info(static_cast<MetricId>(i));
        CHECK(FindByKey(info.key) == &info);
    }
    CHECK(FindByKey("unknown") == nullptr);
}

TEST(format_values) {
    CHECK(FormatValue(Unit::Percent, 23.4) == L"23%");
    CHECK(FormatValue(Unit::Percent, 140) == L"100%");
    CHECK(FormatValue(Unit::Celsius, 54.6) == L"55\x00B0");
    CHECK(FormatValue(Unit::MHz, 4210) == L"4.21 GHz");
    CHECK(FormatValue(Unit::GigaBytes, 12.43) == L"12.4 GB");
    CHECK(FormatValue(Unit::BytesPerSec, 512) == L"512 B/s");
    CHECK(FormatValue(Unit::BytesPerSec, 3.2 * 1024 * 1024) == L"3.2 MB/s");
    CHECK(FormatValue(Unit::BytesPerSec, 210 * 1024) == L"210 KB/s");
    CHECK(FormatValue(Unit::Duration, 3 * 3600 + 7 * 60) == L"3:07");
    CHECK(FormatValue(Unit::Duration, 2 * 86400 + 5 * 3600) == L"2d 05h");
    CHECK(FormatValue(Unit::Count, 312) == L"312");
}

TEST(format_uses_user_decimal_separator) {
    SetDecimalSeparator(L',');
    const bool ok = FormatValue(Unit::MHz, 4210) == L"4,21 GHz" && FormatValue(Unit::BytesPerSec, 3.2 * 1024 * 1024) == L"3,2 MB/s" &&
                    FormatValue(Unit::Percent, 23.4) == L"23%";
    SetDecimalSeparator(L'.');
    CHECK(ok);
}

static FILE* OpenFile(const char* path) {
    FILE* f = nullptr;
    return fopen_s(&f, path, "wb") == 0 ? f : nullptr;
}

int main(int argc, char** argv) {
    const char* junit = nullptr;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--junit") == 0) junit = argv[i + 1];
    }
    std::string xml;
    int failed = 0;
    for (const TestCase& t : Registry()) {
        g_failure.clear();
        t.body();
        const bool ok = g_failure.empty();
        std::printf("[%s] %s%s%s\n", ok ? " OK " : "FAIL", t.name, ok ? "" : "  ", g_failure.c_str());
        xml += "  <testcase classname=\"core\" name=\"" + std::string(t.name) + "\">";
        if (!ok) {
            ++failed;
            xml += "<failure message=\"assert\"><![CDATA[" + g_failure + "]]></failure>";
        }
        xml += "</testcase>\n";
    }
    std::printf("%zu pruebas, %d fallidas\n", Registry().size(), failed);
    if (junit) {
        if (FILE* f = OpenFile(junit)) {
            std::fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite name=\"wram-core\" tests=\"%zu\" failures=\"%d\">\n%s</testsuite>\n",
                         Registry().size(), failed, xml.c_str());
            std::fclose(f);
        }
    }
    return failed == 0 ? 0 : 1;
}
