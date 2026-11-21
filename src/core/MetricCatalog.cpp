#include "MetricCatalog.h"

#include <array>

#include "I18n.h"

namespace wram {
namespace {

constexpr std::array<MetricInfo, kMetricCount> kCatalog{{
    {MetricId::CpuLoad, "cpu", "Uso de CPU", "Contador de utilidad del procesador", L"CPU", Unit::Percent, 90, 100, false, false},
    {MetricId::CpuTemp, "cputemp", "Temperatura de CPU", "Zona térmica ACPI de la placa", L"T.CPU", Unit::Celsius, 85, 120, false, true},
    {MetricId::CpuClock, "cpuclock", "Frecuencia de CPU", "Frecuencia base por rendimiento actual", L"FREQ", Unit::MHz, 0, 0, false, false},
    {MetricId::RamLoad, "ram", "Uso de RAM", "Memoria física en uso", L"RAM", Unit::Percent, 90, 100, false, false},
    {MetricId::RamUsed, "ramused", "RAM en uso", "Memoria física en uso, en GB", L"MEM", Unit::GigaBytes, 0, 1024, false, false},
    {MetricId::GpuLoad, "gpu", "Uso de GPU", "Motor 3D, igual que el Administrador de tareas", L"GPU", Unit::Percent, 95, 100, false, false},
    {MetricId::GpuTemp, "gputemp", "Temperatura de GPU", "Sensor que el driver expone a Windows", L"T.GPU", Unit::Celsius, 80, 120, false, false},
    {MetricId::VramUsed, "vram", "VRAM en uso", "Memoria dedicada de la GPU", L"VRAM", Unit::GigaBytes, 0, 256, false, false},
    {MetricId::DiskActivity, "disk", "Actividad de disco", "Tiempo activo del disco más ocupado", L"DISCO", Unit::Percent, 95, 100, false, false},
    {MetricId::NetDown, "netdown", "Descarga", "Suma de todas las interfaces de red", L"BAJ", Unit::BytesPerSec, 0, 10000, false, false},
    {MetricId::NetUp, "netup", "Subida", "Suma de todas las interfaces de red", L"SUB", Unit::BytesPerSec, 0, 10000, false, false},
    {MetricId::Battery, "battery", "Batería", "Estado de energía del sistema", L"BAT", Unit::Percent, 20, 100, true, false},
    {MetricId::Uptime, "uptime", "Tiempo encendido", "Desde el último arranque", L"ON", Unit::Duration, 0, 0, false, false},
    {MetricId::Processes, "procs", "Procesos", "Procesos en ejecución", L"PROC", Unit::Count, 0, 10000, false, false},
}};

struct LocalizedText {
    const char* name;
    const char* source;
    const wchar_t* label;
};
using MetricTexts = std::array<LocalizedText, kMetricCount>;

// Traducciones del catálogo para los idiomas distintos del español, en el orden de kCatalog.
constexpr MetricTexts kEnglish{{
    {"CPU usage", "Processor utility counter", L"CPU"},
    {"CPU temperature", "Motherboard ACPI thermal zone", L"T.CPU"},
    {"CPU clock", "Base frequency times current performance", L"FREQ"},
    {"RAM usage", "Physical memory in use", L"RAM"},
    {"RAM in use", "Physical memory in use, in GB", L"MEM"},
    {"GPU usage", "3D engine, same as Task Manager", L"GPU"},
    {"GPU temperature", "Sensor exposed by the driver to Windows", L"T.GPU"},
    {"VRAM in use", "Dedicated GPU memory", L"VRAM"},
    {"Disk activity", "Active time of the busiest disk", L"DISK"},
    {"Download", "Sum of all network interfaces", L"DOWN"},
    {"Upload", "Sum of all network interfaces", L"UP"},
    {"Battery", "System power status", L"BAT"},
    {"Uptime", "Since the last boot", L"ON"},
    {"Processes", "Running processes", L"PROC"},
}};
constexpr MetricTexts kPortuguese{{
    {"Uso da CPU", "Contador de utilização do processador", L"CPU"},
    {"Temperatura da CPU", "Zona térmica ACPI da placa-mãe", L"T.CPU"},
    {"Frequência da CPU", "Frequência base vezes o desempenho atual", L"FREQ"},
    {"Uso da RAM", "Memória física em uso", L"RAM"},
    {"RAM em uso", "Memória física em uso, em GB", L"MEM"},
    {"Uso da GPU", "Motor 3D, igual ao Gerenciador de Tarefas", L"GPU"},
    {"Temperatura da GPU", "Sensor que o driver expõe ao Windows", L"T.GPU"},
    {"VRAM em uso", "Memória dedicada da GPU", L"VRAM"},
    {"Atividade do disco", "Tempo ativo do disco mais ocupado", L"DISCO"},
    {"Download", "Soma de todas as interfaces de rede", L"DOWN"},
    {"Upload", "Soma de todas as interfaces de rede", L"UP"},
    {"Bateria", "Estado de energia do sistema", L"BAT"},
    {"Tempo ligado", "Desde a última inicialização", L"ON"},
    {"Processos", "Processos em execução", L"PROC"},
}};
constexpr MetricTexts kFrench{{
    {"Utilisation CPU", "Compteur d’utilité du processeur", L"CPU"},
    {"Température CPU", "Zone thermique ACPI de la carte mère", L"T.CPU"},
    {"Fréquence CPU", "Fréquence de base multipliée par la performance actuelle", L"FRÉQ"},
    {"Utilisation RAM", "Mémoire physique utilisée", L"RAM"},
    {"RAM utilisée", "Mémoire physique utilisée, en GB", L"MÉM"},
    {"Utilisation GPU", "Moteur 3D, comme le Gestionnaire des tâches", L"GPU"},
    {"Température GPU", "Capteur exposé par le pilote à Windows", L"T.GPU"},
    {"VRAM utilisée", "Mémoire dédiée du GPU", L"VRAM"},
    {"Activité disque", "Temps actif du disque le plus occupé", L"DISQ"},
    {"Réception", "Somme de toutes les interfaces réseau", L"RÉC"},
    {"Envoi", "Somme de toutes les interfaces réseau", L"ENV"},
    {"Batterie", "État d’alimentation du système", L"BAT"},
    {"Temps de fonctionnement", "Depuis le dernier démarrage", L"ON"},
    {"Processus", "Processus en cours d’exécution", L"PROC"},
}};
constexpr MetricTexts kChinese{{
    {"CPU 使用率", "处理器利用率计数器", L"CPU"},
    {"CPU 温度", "主板 ACPI 热区", L"CPU温"},
    {"CPU 频率", "基础频率乘以当前性能", L"频率"},
    {"内存使用率", "正在使用的物理内存", L"内存"},
    {"已用内存", "正在使用的物理内存（GB）", L"已用"},
    {"GPU 使用率", "3D 引擎，与任务管理器相同", L"GPU"},
    {"GPU 温度", "驱动向 Windows 提供的传感器", L"GPU温"},
    {"已用显存", "GPU 专用内存", L"显存"},
    {"磁盘活动", "最繁忙磁盘的活动时间", L"磁盘"},
    {"下载", "所有网络接口之和", L"下载"},
    {"上传", "所有网络接口之和", L"上传"},
    {"电池", "系统电源状态", L"电池"},
    {"运行时间", "自上次启动以来", L"运行"},
    {"进程", "正在运行的进程", L"进程"},
}};

constexpr MetricTexts kJapanese{{
    {"CPU 使用率", "プロセッサ使用率カウンター", L"CPU"},
    {"CPU 温度", "マザーボードの ACPI サーマルゾーン", L"CPU温"},
    {"CPU クロック", "ベース周波数 × 現在のパフォーマンス", L"周波数"},
    {"メモリ使用率", "使用中の物理メモリ", L"メモリ"},
    {"使用中のメモリ", "使用中の物理メモリ（GB）", L"使用中"},
    {"GPU 使用率", "3D エンジン、タスク マネージャーと同じ", L"GPU"},
    {"GPU 温度", "ドライバーが Windows に提供するセンサー", L"GPU温"},
    {"使用中の VRAM", "GPU 専用メモリ", L"VRAM"},
    {"ディスク使用率", "最も使用率の高いディスクのアクティブ時間", L"ディスク"},
    {"ダウンロード", "すべてのネットワーク インターフェイスの合計", L"受信"},
    {"アップロード", "すべてのネットワーク インターフェイスの合計", L"送信"},
    {"バッテリー", "システムの電源状態", L"電池"},
    {"稼働時間", "前回の起動から", L"稼働"},
    {"プロセス", "実行中のプロセス", L"PROC"},
}};

const LocalizedText* Localized(MetricId id) {
    switch (CurrentLang()) {
        case Lang::En: return &kEnglish[ToIndex(id)];
        case Lang::Pt: return &kPortuguese[ToIndex(id)];
        case Lang::Fr: return &kFrench[ToIndex(id)];
        case Lang::Zh: return &kChinese[ToIndex(id)];
        case Lang::Ja: return &kJapanese[ToIndex(id)];
        default: return nullptr;
    }
}

constexpr bool CatalogIsOrdered() {
    for (size_t i = 0; i < kCatalog.size(); ++i) {
        if (ToIndex(kCatalog[i].id) != i) return false;
    }
    return true;
}
static_assert(CatalogIsOrdered(), "El catálogo debe seguir el orden de MetricId");

}  // namespace

const MetricInfo& Info(MetricId id) { return kCatalog[ToIndex(id)]; }

const char* NameOf(const MetricInfo& info) {
    const LocalizedText* t = Localized(info.id);
    return t ? t->name : info.name;
}
const char* SourceOf(const MetricInfo& info) {
    const LocalizedText* t = Localized(info.id);
    return t ? t->source : info.source;
}
const wchar_t* LabelOf(const MetricInfo& info) {
    const LocalizedText* t = Localized(info.id);
    return t ? t->label : info.label;
}

const MetricInfo* FindByKey(std::string_view key) {
    for (const auto& info : kCatalog) {
        if (key == info.key) return &info;
    }
    return nullptr;
}

double ThresholdScale(Unit unit) {
    switch (unit) {
        case Unit::BytesPerSec: return 1024.0 * 1024.0;  // umbral en MB/s
        case Unit::GigaBytes: return 1.0;
        default: return 1.0;
    }
}

bool IsAlert(const MetricInfo& info, double rawValue, double threshold) {
    if (info.thresholdMax <= 0 || threshold <= 0) return false;
    const double limit = threshold * ThresholdScale(info.unit);
    return info.alertBelow ? rawValue <= limit : rawValue >= limit;
}

}  // namespace wram
