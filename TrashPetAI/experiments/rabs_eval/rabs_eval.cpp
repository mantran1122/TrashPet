// RABS (Risk-Aware Budget Selection) evaluation harness.
//
// Standalone C++17 program (no Qt dependency) used to generate the
// experimental numbers for Section 6/7 of the ICEIR paper. It mirrors the
// selection logic already shipped in TrashPetAI/cleanupplan.cpp (OldestFirst,
// LargestFirst, ClosestFit) and adds two new policies (Random, RABS) plus an
// ablation variant (RABS-NoRisk) on top of a synthetic-but-reproducible
// Recycle Bin population model. No real files are created or deleted; all
// deletion outcomes are simulated via a seeded failure model so the harness
// is safe to re-run any number of times.
//
// Build:  g++ -O2 -std=c++17 -o rabs_eval.exe rabs_eval.cpp
// Run:    ./rabs_eval.exe [repetitions] [outputDir]

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Domain model
// ---------------------------------------------------------------------------

enum class Category { Cache, Media, Archive, Installer, Document, ProjectConfig, SystemBackup };

const char *categoryName(Category c) {
    switch (c) {
    case Category::Cache: return "cache";
    case Category::Media: return "media";
    case Category::Archive: return "archive";
    case Category::Installer: return "installer";
    case Category::Document: return "document";
    case Category::ProjectConfig: return "project_config";
    case Category::SystemBackup: return "system_backup";
    }
    return "?";
}

// Base risk-of-being-needed-again score in [0,1], by file category.
double baseCategoryRisk(Category c) {
    switch (c) {
    case Category::Cache: return 0.05;
    case Category::Media: return 0.10;
    case Category::Archive: return 0.20;
    case Category::Installer: return 0.15;
    case Category::Document: return 0.40;
    case Category::ProjectConfig: return 0.65;
    case Category::SystemBackup: return 0.75;
    }
    return 0.0;
}

// Probability that an item of this category is a hard-protected item
// (must NEVER be auto-deleted, e.g. .env/.pem/.key, active project config,
// unfinished tax/contract documents, unverified system backups).
double protectedProbability(Category c) {
    switch (c) {
    case Category::Cache: return 0.0;
    case Category::Media: return 0.005;
    case Category::Archive: return 0.01;
    case Category::Installer: return 0.0;
    case Category::Document: return 0.03;
    case Category::ProjectConfig: return 0.35;
    case Category::SystemBackup: return 0.50;
    }
    return 0.0;
}

struct Item {
    uint64_t sizeBytes = 0;
    int ageDays = 0;
    Category category = Category::Cache;
    double riskScore = 0.0;   // continuous, [0,1]
    bool protectedFlag = false;
};

constexpr uint64_t kMaxFileBytes = 1024ULL * 1024ULL * 1024ULL; // 1 GB skip threshold (matches HungerCfg)
constexpr double kRiskThreshold = 0.40; // "risky" cutoff used for Risky Selection Rate + RABS phase split

// ---------------------------------------------------------------------------
// Scenario definitions
// ---------------------------------------------------------------------------

struct CategoryWeight { Category cat; double weight; };

struct Scenario {
    std::string name;
    std::vector<CategoryWeight> mix; // must sum to ~1.0
    int poolSize;
    double sizeMuLog[7];             // ln(mean bytes) per Category index
    double sizeSigmaLog[7];
};

int catIndex(Category c) { return static_cast<int>(c); }

Scenario makeScenario(std::string name, std::vector<CategoryWeight> mix, int poolSize) {
    Scenario s;
    s.name = std::move(name);
    s.mix = std::move(mix);
    s.poolSize = poolSize;
    // Rough real-world size priors per category (bytes), shared across scenarios.
    const double mu[7] = {
        std::log(2.0e6),    // cache
        std::log(30.0e6),   // media
        std::log(80.0e6),   // archive
        std::log(150.0e6),  // installer
        std::log(1.5e6),    // document
        std::log(0.5e6),    // project_config
        std::log(300.0e6),  // system_backup
    };
    const double sigma[7] = {1.2, 1.5, 1.3, 1.0, 1.0, 1.4, 1.2};
    for (int i = 0; i < 7; ++i) { s.sizeMuLog[i] = mu[i]; s.sizeSigmaLog[i] = sigma[i]; }
    return s;
}

std::vector<Scenario> buildScenarios() {
    std::vector<Scenario> v;
    v.push_back(makeScenario("S1_MediaHeavyPersonal", {
        {Category::Media, 0.70}, {Category::Archive, 0.10}, {Category::Cache, 0.10},
        {Category::Document, 0.05}, {Category::ProjectConfig, 0.03}, {Category::SystemBackup, 0.02}
    }, 400));
    v.push_back(makeScenario("S2_DevWorkspace", {
        {Category::ProjectConfig, 0.35}, {Category::Cache, 0.30}, {Category::Archive, 0.15},
        {Category::Document, 0.10}, {Category::Media, 0.05}, {Category::SystemBackup, 0.05}
    }, 400));
    v.push_back(makeScenario("S3_DownloadsMixed", {
        {Category::Installer, 0.30}, {Category::Archive, 0.20}, {Category::Media, 0.20},
        {Category::Document, 0.15}, {Category::Cache, 0.10}, {Category::ProjectConfig, 0.05}
    }, 400));
    v.push_back(makeScenario("S4_DocumentHeavy", {
        {Category::Document, 0.55}, {Category::Archive, 0.15}, {Category::Media, 0.10},
        {Category::Cache, 0.05}, {Category::ProjectConfig, 0.05}, {Category::SystemBackup, 0.10}
    }, 400));
    v.push_back(makeScenario("S5_NearFullDiskEmergency", {
        {Category::Cache, 0.45}, {Category::Media, 0.25}, {Category::Archive, 0.15},
        {Category::Document, 0.05}, {Category::ProjectConfig, 0.05}, {Category::SystemBackup, 0.05}
    }, 400));
    return v;
}

struct BudgetLevel { std::string name; double fraction; };
std::vector<BudgetLevel> buildBudgetLevels() {
    return { {"B1_Low10", 0.10}, {"B2_Medium30", 0.30}, {"B3_High60", 0.60}, {"B4_Aggressive90", 0.90} };
}

// ---------------------------------------------------------------------------
// Population generation (reproducible via seeded RNG)
// ---------------------------------------------------------------------------

Category sampleCategory(const Scenario &s, std::mt19937_64 &rng) {
    std::discrete_distribution<int> dist;
    std::vector<double> weights;
    for (auto &cw : s.mix) weights.push_back(cw.weight);
    dist = std::discrete_distribution<int>(weights.begin(), weights.end());
    return s.mix[dist(rng)].cat;
}

std::vector<Item> generatePool(const Scenario &s, std::mt19937_64 &rng) {
    std::vector<Item> items;
    items.reserve(s.poolSize);
    std::exponential_distribution<double> ageDist(1.0 / 15.0); // mean 15 days
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    for (int i = 0; i < s.poolSize; ++i) {
        Item it;
        it.category = sampleCategory(s, rng);
        const int ci = catIndex(it.category);
        std::lognormal_distribution<double> sizeDist(s.sizeMuLog[ci], s.sizeSigmaLog[ci]);
        it.sizeBytes = static_cast<uint64_t>(std::max(1024.0, sizeDist(rng)));
        it.ageDays = std::min(180, static_cast<int>(ageDist(rng)));

        it.protectedFlag = unit(rng) < protectedProbability(it.category);

        double risk = baseCategoryRisk(it.category);
        if (it.ageDays < 7) risk += (7 - it.ageDays) / 7.0 * 0.30; // recently deleted -> higher "still needed" risk
        if (it.protectedFlag) risk = 1.0;
        it.riskScore = std::clamp(risk, 0.0, 1.0);

        items.push_back(it);
    }
    return items;
}

// ---------------------------------------------------------------------------
// Selection policies
// ---------------------------------------------------------------------------

enum class Policy { OldestFirst, LargestFirst, ClosestFit, Random, RABS, RABS_NoRisk };

const char *policyName(Policy p) {
    switch (p) {
    case Policy::OldestFirst: return "OldestFirst";
    case Policy::LargestFirst: return "LargestFirst";
    case Policy::ClosestFit: return "ClosestFit";
    case Policy::Random: return "Random";
    case Policy::RABS: return "RABS";
    case Policy::RABS_NoRisk: return "RABS_NoRisk";
    }
    return "?";
}

struct PlanResult {
    std::vector<int> selectedIdx; // indices into the eligible pool
    uint64_t plannedBytes = 0;
};

// Ported 1:1 from cleanupplan.cpp's selectClosestFit (best-fit-decreasing greedy).
void selectClosestFit(std::vector<int> pool, const std::vector<Item> &items, uint64_t budget, PlanResult &out) {
    int64_t remaining = static_cast<int64_t>(budget);
    while (remaining > 0 && !pool.empty()) {
        int fitIdx = -1; int64_t fitSize = -1;
        int overIdx = -1; int64_t overSize = std::numeric_limits<int64_t>::max();
        for (size_t k = 0; k < pool.size(); ++k) {
            const int64_t sz = static_cast<int64_t>(items[pool[k]].sizeBytes);
            if (sz <= remaining) { if (sz > fitSize) { fitSize = sz; fitIdx = static_cast<int>(k); } }
            else if (sz < overSize) { overSize = sz; overIdx = static_cast<int>(k); }
        }
        const int chosen = (fitIdx != -1) ? fitIdx : overIdx;
        if (chosen == -1) break;
        const int itemIdx = pool[chosen];
        pool.erase(pool.begin() + chosen);
        out.selectedIdx.push_back(itemIdx);
        out.plannedBytes += items[itemIdx].sizeBytes;
        remaining -= static_cast<int64_t>(items[itemIdx].sizeBytes);
    }
}

void selectSorted(std::vector<int> pool, const std::vector<Item> &items, uint64_t budget, bool largestFirst, PlanResult &out) {
    std::sort(pool.begin(), pool.end(), [&](int a, int b) {
        if (largestFirst) return items[a].sizeBytes > items[b].sizeBytes;
        return items[a].ageDays > items[b].ageDays; // older (bigger ageDays) first
    });
    int64_t remaining = static_cast<int64_t>(budget);
    for (int idx : pool) {
        if (remaining <= 0) break;
        out.selectedIdx.push_back(idx);
        out.plannedBytes += items[idx].sizeBytes;
        remaining -= static_cast<int64_t>(items[idx].sizeBytes);
    }
}

void selectRandom(std::vector<int> pool, const std::vector<Item> &items, uint64_t budget, std::mt19937_64 &rng, PlanResult &out) {
    std::shuffle(pool.begin(), pool.end(), rng);
    int64_t remaining = static_cast<int64_t>(budget);
    for (int idx : pool) {
        if (remaining <= 0) break;
        out.selectedIdx.push_back(idx);
        out.plannedBytes += items[idx].sizeBytes;
        remaining -= static_cast<int64_t>(items[idx].sizeBytes);
    }
}

// RABS: hard-excludes protected items, then greedily prefers low-risk items
// (best-fit within the low-risk pool first), only reaching into the
// higher-risk pool if the budget cannot otherwise be met.
// useRisk=false collapses this to closest-fit-over-non-protected-items,
// which is the ablation variant (RABS_NoRisk): keeps the hard constraint,
// drops the continuous risk-aware ranking.
void selectRABS(const std::vector<int> &eligiblePool, const std::vector<Item> &items, uint64_t budget, bool useRisk, PlanResult &out) {
    std::vector<int> lowRisk, highRisk;
    for (int idx : eligiblePool) {
        if (items[idx].protectedFlag) continue; // hard constraint, never a candidate
        if (useRisk && items[idx].riskScore >= kRiskThreshold) highRisk.push_back(idx);
        else lowRisk.push_back(idx);
    }

    int64_t remaining = static_cast<int64_t>(budget);

    auto bestFitPass = [&](std::vector<int> &pool) {
        while (remaining > 0 && !pool.empty()) {
            int fitIdx = -1; int64_t fitSize = -1;
            int overIdx = -1; int64_t overSize = std::numeric_limits<int64_t>::max();
            for (size_t k = 0; k < pool.size(); ++k) {
                const int64_t sz = static_cast<int64_t>(items[pool[k]].sizeBytes);
                if (sz <= remaining) { if (sz > fitSize) { fitSize = sz; fitIdx = static_cast<int>(k); } }
                else if (sz < overSize) { overSize = sz; overIdx = static_cast<int>(k); }
            }
            const int chosen = (fitIdx != -1) ? fitIdx : overIdx;
            if (chosen == -1) break;
            const int itemIdx = pool[chosen];
            pool.erase(pool.begin() + chosen);
            out.selectedIdx.push_back(itemIdx);
            out.plannedBytes += items[itemIdx].sizeBytes;
            remaining -= static_cast<int64_t>(items[itemIdx].sizeBytes);
        }
    };

    bestFitPass(lowRisk);
    if (remaining > 0) bestFitPass(highRisk);
}

PlanResult buildPlan(Policy p, const std::vector<int> &eligible, const std::vector<Item> &items, uint64_t budget, std::mt19937_64 &rng) {
    PlanResult out;
    switch (p) {
    case Policy::OldestFirst: selectSorted(eligible, items, budget, false, out); break;
    case Policy::LargestFirst: selectSorted(eligible, items, budget, true, out); break;
    case Policy::ClosestFit: selectClosestFit(eligible, items, budget, out); break;
    case Policy::Random: selectRandom(eligible, items, budget, rng, out); break;
    case Policy::RABS: selectRABS(eligible, items, budget, true, out); break;
    case Policy::RABS_NoRisk: selectRABS(eligible, items, budget, false, out); break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Deletion outcome simulation (no real files touched)
// ---------------------------------------------------------------------------

double deletionFailureProb(const Item &it) {
    double p = 0.02; // base friction: locked handle / already purged / permission race
    if (it.ageDays > 90) p += 0.05;
    if (it.sizeBytes > 500ULL * 1024 * 1024) p += 0.01;
    return std::min(p, 0.20);
}

// ---------------------------------------------------------------------------
// Per-run record + aggregation
// ---------------------------------------------------------------------------

struct RunRecord {
    std::string scenario, budgetLevel, policy;
    int rep = 0;
    uint64_t budgetBytes = 0;
    int poolSize = 0, eligibleSize = 0, protectedInPool = 0;
    int selectedCount = 0;
    uint64_t plannedBytes = 0;
    uint64_t actualReclaimedBytes = 0;
    double budgetError = 0.0;        // actual - target (signed)
    double overshootRatio = 0.0;
    int achieved = 0;
    int riskySelectedCount = 0;
    double riskySelectionRate = 0.0;
    int protectedViolationCount = 0;
    double protectedViolationRate = 0.0; // violationCount / max(1, protectedInPool)
    int deletionSuccessCount = 0;
    double deletionSuccessRate = 1.0;
    double planningTimeUs = 0.0;
};

RunRecord runOnce(const Scenario &scn, const BudgetLevel &bl, Policy pol, int rep, uint64_t baseSeed) {
    std::mt19937_64 poolRng(baseSeed);
    std::vector<Item> pool = generatePool(scn, poolRng);

    uint64_t totalBytes = 0;
    for (auto &it : pool) totalBytes += it.sizeBytes;
    const uint64_t budgetBytes = static_cast<uint64_t>(totalBytes * bl.fraction);

    std::vector<int> eligible;
    int protectedInPool = 0;
    for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
        if (pool[i].sizeBytes > kMaxFileBytes) continue; // same >1GB skip as production CleanupPlan
        eligible.push_back(i);
        if (pool[i].protectedFlag) ++protectedInPool;
    }

    std::mt19937_64 policyRng(baseSeed ^ 0x9E3779B97F4A7C15ULL);

    const auto t0 = std::chrono::high_resolution_clock::now();
    PlanResult plan = buildPlan(pol, eligible, pool, budgetBytes, policyRng);
    const auto t1 = std::chrono::high_resolution_clock::now();

    std::mt19937_64 delRng(baseSeed ^ 0xD1B54A32D192ED03ULL);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    RunRecord r;
    r.scenario = scn.name; r.budgetLevel = bl.name; r.policy = policyName(pol);
    r.rep = rep; r.budgetBytes = budgetBytes;
    r.poolSize = static_cast<int>(pool.size());
    r.eligibleSize = static_cast<int>(eligible.size());
    r.protectedInPool = protectedInPool;
    r.selectedCount = static_cast<int>(plan.selectedIdx.size());
    r.plannedBytes = plan.plannedBytes;
    r.planningTimeUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

    uint64_t actual = 0;
    int successCount = 0, riskyCount = 0, violationCount = 0;
    for (int idx : plan.selectedIdx) {
        const Item &it = pool[idx];
        const bool success = unit(delRng) >= deletionFailureProb(it);
        if (success) { actual += it.sizeBytes; ++successCount; }
        if (it.riskScore >= kRiskThreshold) ++riskyCount;
        if (it.protectedFlag) ++violationCount;
    }
    r.actualReclaimedBytes = actual;
    r.deletionSuccessCount = successCount;
    r.deletionSuccessRate = r.selectedCount > 0 ? double(successCount) / r.selectedCount : 1.0;
    r.riskySelectedCount = riskyCount;
    r.riskySelectionRate = r.selectedCount > 0 ? double(riskyCount) / r.selectedCount : 0.0;
    r.protectedViolationCount = violationCount;
    r.protectedViolationRate = double(violationCount) / std::max(1, protectedInPool);

    const double target = static_cast<double>(budgetBytes);
    r.budgetError = static_cast<double>(actual) - target;
    r.overshootRatio = target > 0 ? std::max(0.0, static_cast<double>(actual) - target) / target : 0.0;
    r.achieved = (actual >= budgetBytes) ? 1 : 0;

    return r;
}

// ---------------------------------------------------------------------------
// Stats helpers
// ---------------------------------------------------------------------------

double mean(const std::vector<double> &v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}
double stddev(const std::vector<double> &v) {
    if (v.size() < 2) return 0.0;
    const double m = mean(v);
    double s = 0.0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / (v.size() - 1));
}
double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(std::ceil(p * v.size())) - 1;
    idx = std::min(idx, v.size() - 1);
    return v[idx];
}
double medianOf(std::vector<double> v) { return percentile(v, 0.5); }

// ---------------------------------------------------------------------------
// CSV writers
// ---------------------------------------------------------------------------

void writeRawCsv(const std::string &path, const std::vector<RunRecord> &rows) {
    std::ofstream f(path);
    f << "scenario,budget_level,policy,rep,budget_bytes,pool_size,eligible_size,protected_in_pool,"
         "selected_count,planned_bytes,actual_reclaimed_bytes,budget_error,overshoot_ratio,achieved,"
         "risky_selected_count,risky_selection_rate,protected_violation_count,protected_violation_rate,"
         "deletion_success_count,deletion_success_rate,planning_time_us\n";
    f << std::fixed << std::setprecision(6);
    for (auto &r : rows) {
        f << r.scenario << "," << r.budgetLevel << "," << r.policy << "," << r.rep << ","
          << r.budgetBytes << "," << r.poolSize << "," << r.eligibleSize << "," << r.protectedInPool << ","
          << r.selectedCount << "," << r.plannedBytes << "," << r.actualReclaimedBytes << ","
          << r.budgetError << "," << r.overshootRatio << "," << r.achieved << ","
          << r.riskySelectedCount << "," << r.riskySelectionRate << ","
          << r.protectedViolationCount << "," << r.protectedViolationRate << ","
          << r.deletionSuccessCount << "," << r.deletionSuccessRate << "," << r.planningTimeUs << "\n";
    }
}

struct GroupKey {
    std::string scenario, budgetLevel, policy;
    bool operator<(const GroupKey &o) const {
        return std::tie(scenario, budgetLevel, policy) < std::tie(o.scenario, o.budgetLevel, o.policy);
    }
};

void writeSummaryByCombo(const std::string &path, const std::vector<RunRecord> &rows) {
    std::map<GroupKey, std::vector<const RunRecord *>> groups;
    for (auto &r : rows) groups[{r.scenario, r.budgetLevel, r.policy}].push_back(&r);

    std::ofstream f(path);
    f << "scenario,budget_level,policy,n_reps,"
         "budget_error_abs_mean,budget_error_abs_std,"
         "overshoot_ratio_mean,overshoot_ratio_std,"
         "target_achievement_rate,"
         "risky_selection_rate,"
         "protected_violation_rate,"
         "planning_time_us_mean,planning_time_us_median,planning_time_us_p95,"
         "actual_reclaimed_bytes_mean,actual_reclaimed_bytes_std,"
         "deletion_success_rate\n";
    f << std::fixed << std::setprecision(6);

    for (auto &[key, recs] : groups) {
        std::vector<double> absErr, overshoot, achieved, risky, violation, planTime, reclaimed, delSuccess;
        for (auto *r : recs) {
            absErr.push_back(std::fabs(r->budgetError));
            overshoot.push_back(r->overshootRatio);
            achieved.push_back(r->achieved);
            risky.push_back(r->riskySelectionRate);
            violation.push_back(r->protectedViolationRate);
            planTime.push_back(r->planningTimeUs);
            reclaimed.push_back(static_cast<double>(r->actualReclaimedBytes));
            delSuccess.push_back(r->deletionSuccessRate);
        }
        f << key.scenario << "," << key.budgetLevel << "," << key.policy << "," << recs.size() << ","
          << mean(absErr) << "," << stddev(absErr) << ","
          << mean(overshoot) << "," << stddev(overshoot) << ","
          << mean(achieved) << ","
          << mean(risky) << ","
          << mean(violation) << ","
          << mean(planTime) << "," << medianOf(planTime) << "," << percentile(planTime, 0.95) << ","
          << mean(reclaimed) << "," << stddev(reclaimed) << ","
          << mean(delSuccess) << "\n";
    }
}

void writeSummaryByPolicy(const std::string &path, const std::vector<RunRecord> &rows) {
    std::map<std::string, std::vector<const RunRecord *>> groups;
    for (auto &r : rows) groups[r.policy].push_back(&r);

    std::ofstream f(path);
    f << "policy,n_reps,"
         "budget_error_abs_mean,budget_error_abs_std,"
         "overshoot_ratio_mean,overshoot_ratio_std,"
         "target_achievement_rate,"
         "risky_selection_rate,"
         "protected_violation_rate,"
         "planning_time_us_mean,planning_time_us_median,planning_time_us_p95,"
         "actual_reclaimed_bytes_mean,actual_reclaimed_bytes_std,"
         "deletion_success_rate\n";
    f << std::fixed << std::setprecision(6);

    for (auto &[policy, recs] : groups) {
        std::vector<double> absErr, overshoot, achieved, risky, violation, planTime, reclaimed, delSuccess;
        for (auto *r : recs) {
            absErr.push_back(std::fabs(r->budgetError));
            overshoot.push_back(r->overshootRatio);
            achieved.push_back(r->achieved);
            risky.push_back(r->riskySelectionRate);
            violation.push_back(r->protectedViolationRate);
            planTime.push_back(r->planningTimeUs);
            reclaimed.push_back(static_cast<double>(r->actualReclaimedBytes));
            delSuccess.push_back(r->deletionSuccessRate);
        }
        f << policy << "," << recs.size() << ","
          << mean(absErr) << "," << stddev(absErr) << ","
          << mean(overshoot) << "," << stddev(overshoot) << ","
          << mean(achieved) << ","
          << mean(risky) << ","
          << mean(violation) << ","
          << mean(planTime) << "," << medianOf(planTime) << "," << percentile(planTime, 0.95) << ","
          << mean(reclaimed) << "," << stddev(reclaimed) << ","
          << mean(delSuccess) << "\n";
    }
}

void writeAnomalies(const std::string &path, const std::vector<RunRecord> &rows) {
    std::ofstream f(path);
    f << "# RABS eval anomaly log\n\n";

    f << "## Runs with Deletion Success Rate < 100%\n";
    int delAnom = 0;
    for (auto &r : rows) {
        if (r.deletionSuccessRate < 1.0 && r.selectedCount > 0) {
            f << r.scenario << " | " << r.budgetLevel << " | " << r.policy << " | rep=" << r.rep
              << " | selected=" << r.selectedCount << " | failed="
              << (r.selectedCount - r.deletionSuccessCount)
              << " | success_rate=" << std::fixed << std::setprecision(3) << r.deletionSuccessRate << "\n";
            ++delAnom;
        }
    }
    f << "(" << delAnom << " runs out of " << rows.size() << " had at least one simulated deletion failure)\n\n";

    f << "## Combos with Protected-Item Violation Rate > 0 (grouped)\n";
    std::map<GroupKey, std::vector<double>> viol;
    for (auto &r : rows) viol[{r.scenario, r.budgetLevel, r.policy}].push_back(r.protectedViolationRate);
    for (auto &[key, v] : viol) {
        const double m = mean(v);
        if (m > 0.0) {
            f << key.scenario << " | " << key.budgetLevel << " | " << key.policy
              << " | mean_violation_rate=" << std::fixed << std::setprecision(4) << m << "\n";
        }
    }
    f << "\nExplanation: OldestFirst/LargestFirst/ClosestFit/Random have no concept of a hard-protected\n"
         "item, so they treat protected files as ordinary eligible candidates and select them whenever\n"
         "size/age/chance makes them a good fit. RABS and RABS_NoRisk hard-exclude protected items from\n"
         "the candidate pool before any ranking happens, so their violation rate is expected to be 0 in\n"
         "every combo; a nonzero value there would indicate an implementation bug, not an algorithmic\n"
         "trade-off.\n\n";

    f << "## Combos where RABS did not have the lowest mean Overshoot Ratio among all 6 policies\n";
    std::map<GroupKey, std::map<std::string, double>> overshootByPolicy;
    for (auto &r : rows) overshootByPolicy[{r.scenario, r.budgetLevel, ""}][r.policy] += r.overshootRatio;
    // recompute properly with counts
    std::map<std::pair<std::string,std::string>, std::map<std::string, std::vector<double>>> combo2;
    for (auto &r : rows) combo2[{r.scenario, r.budgetLevel}][r.policy].push_back(r.overshootRatio);
    int loseCount = 0;
    for (auto &[sb, polMap] : combo2) {
        std::string best; double bestVal = 1e18;
        for (auto &[pol, vals] : polMap) {
            const double m = mean(vals);
            if (m < bestVal) { bestVal = m; best = pol; }
        }
        auto itRabs = polMap.find("RABS");
        if (itRabs != polMap.end() && best != "RABS") {
            f << sb.first << " | " << sb.second << " | best=" << best << " (mean_overshoot="
              << std::fixed << std::setprecision(4) << bestVal << ") vs RABS (mean_overshoot="
              << mean(itRabs->second) << ")\n";
            ++loseCount;
        }
    }
    f << "(" << loseCount << " / " << combo2.size() << " combos where a baseline beat RABS on overshoot ratio)\n";
}

// ---------------------------------------------------------------------------
// Machine config dump (best-effort, Windows)
// ---------------------------------------------------------------------------

void writeMachineConfig(const std::string &path, int repetitions, size_t totalMainRuns, size_t totalAblationRuns) {
    std::ofstream f(path);
    f << "# RABS eval run configuration\n\n";
    f << "compiler: g++ (MinGW-w64) -O2 -std=c++17\n";
#if defined(__VERSION__)
    f << "compiler_version: " << __VERSION__ << "\n";
#endif
    f << "repetitions_per_combo: " << repetitions << "\n";
    f << "main_grid: 5 scenarios x 4 budget levels x 4 baseline/production policies + RABS = 5 policies\n";
    f << "total_main_runs: " << totalMainRuns << "\n";
    f << "ablation_grid: 5 scenarios x 4 budget levels x {RABS, RABS_NoRisk}\n";
    f << "total_ablation_runs: " << totalAblationRuns << "\n";
    f << "risk_threshold: " << kRiskThreshold << "\n";
    f << "max_file_bytes_skip_threshold: " << kMaxFileBytes << " (1 GB, matches production HungerCfg::kMaxFileBytes)\n";
    f << "note: synthetic population model, no real files created/deleted; deletion outcomes are a seeded\n"
         "      Bernoulli failure model (see deletionFailureProb in rabs_eval.cpp).\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    int repetitions = 30;
    std::string outDir = ".";
    if (argc > 1) repetitions = std::stoi(argv[1]);
    if (argc > 2) outDir = argv[2];
    fs::create_directories(outDir);

    auto scenarios = buildScenarios();
    auto budgets = buildBudgetLevels();
    std::vector<Policy> mainPolicies = { Policy::OldestFirst, Policy::LargestFirst, Policy::ClosestFit, Policy::Random, Policy::RABS };
    std::vector<Policy> ablationPolicies = { Policy::RABS, Policy::RABS_NoRisk };

    std::vector<RunRecord> mainRows, ablationRows;

    uint64_t seedCounter = 1000003;
    for (auto &scn : scenarios) {
        for (auto &bl : budgets) {
            for (int rep = 0; rep < repetitions; ++rep) {
                const uint64_t baseSeed = seedCounter++;
                for (auto pol : mainPolicies) {
                    mainRows.push_back(runOnce(scn, bl, pol, rep, baseSeed));
                }
            }
        }
    }

    seedCounter = 5000003;
    for (auto &scn : scenarios) {
        for (auto &bl : budgets) {
            for (int rep = 0; rep < repetitions; ++rep) {
                const uint64_t baseSeed = seedCounter++;
                for (auto pol : ablationPolicies) {
                    ablationRows.push_back(runOnce(scn, bl, pol, rep, baseSeed));
                }
            }
        }
    }

    writeRawCsv(outDir + "/rabs_raw_results.csv", mainRows);
    writeSummaryByCombo(outDir + "/rabs_summary_by_combo.csv", mainRows);
    writeSummaryByPolicy(outDir + "/rabs_summary_by_policy.csv", mainRows);
    writeAnomalies(outDir + "/rabs_anomalies.log", mainRows);

    writeRawCsv(outDir + "/rabs_ablation_raw.csv", ablationRows);
    writeSummaryByPolicy(outDir + "/rabs_ablation_summary.csv", ablationRows);

    writeMachineConfig(outDir + "/rabs_run_config.log", repetitions, mainRows.size(), ablationRows.size());

    std::cout << "Done. Main runs: " << mainRows.size() << ", ablation runs: " << ablationRows.size() << "\n";
    std::cout << "Output written to: " << outDir << "\n";
    return 0;
}
