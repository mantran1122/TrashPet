import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

# ---- palette (validated categorical slots 1-5, fixed order) ----
COLORS = {
    "OldestFirst":  "#2a78d6",  # blue
    "LargestFirst": "#008300",  # green
    "ClosestFit":   "#e87ba4",  # magenta
    "Random":       "#eda100",  # yellow
    "RABS":         "#1baf7a",  # aqua
}
POLICY_ORDER = ["OldestFirst", "LargestFirst", "ClosestFit", "Random", "RABS"]

INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
INK_MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
SURFACE = "#fcfcfb"

plt.rcParams.update({
    "font.family": "Segoe UI",
    "text.color": INK,
    "axes.edgecolor": AXIS,
    "axes.labelcolor": INK_SECONDARY,
    "xtick.color": INK_MUTED,
    "ytick.color": INK_MUTED,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
})

by_policy = pd.read_csv("rabs_summary_by_policy.csv")
by_combo = pd.read_csv("rabs_summary_by_combo.csv")

# ============================================================
# Chart 1: Overshoot Ratio (mean) by Policy
# ============================================================
fig, ax = plt.subplots(figsize=(7, 4.5), dpi=200)
d = by_policy.set_index("policy").loc[POLICY_ORDER]
vals = d["overshoot_ratio_mean"] * 100.0
bars = ax.bar(POLICY_ORDER, vals, width=0.6,
               color=[COLORS[p] for p in POLICY_ORDER], zorder=3)

ax.yaxis.grid(True, color=GRID, linewidth=1, zorder=0)
ax.set_axisbelow(True)
for spine in ["top", "right", "left"]:
    ax.spines[spine].set_visible(False)
ax.spines["bottom"].set_color(AXIS)
ax.tick_params(axis="both", length=0)
ax.set_ylabel("Overshoot Ratio, mean (%)")
ax.set_title("Overshoot Ratio by Policy (gộp 5 kịch bản x 4 mức ngân sách, n=600/policy)",
              color=INK, fontsize=11, pad=14, loc="left")

for rect, v in zip(bars, vals):
    label = f"{v:.4f}%" if v < 0.01 else f"{v:.2f}%"
    ax.annotate(label, (rect.get_x() + rect.get_width() / 2, rect.get_height()),
                xytext=(0, 4), textcoords="offset points", ha="center",
                fontsize=9, color=INK_SECONDARY)

ax.set_ylim(0, max(vals) * 1.25)
fig.tight_layout()
fig.savefig("chart_overshoot_by_policy.png")
plt.close(fig)

# ============================================================
# Chart 2: Planning Time (mean, us) by Scenario, grouped by Policy
# ============================================================
agg = (by_combo.groupby(["scenario", "policy"])["planning_time_us_mean"]
       .mean().reset_index())
scenarios = ["S1_MediaHeavyPersonal", "S2_DevWorkspace", "S3_DownloadsMixed",
             "S4_DocumentHeavy", "S5_NearFullDiskEmergency"]
scenario_labels = ["S1 Media-\nHeavy", "S2 Dev-\nWorkspace", "S3 Downloads-\nMixed",
                    "S4 Document-\nHeavy", "S5 Near-Full-\nDisk Emergency"]

fig, ax = plt.subplots(figsize=(9.5, 5), dpi=200)
n_pol = len(POLICY_ORDER)
group_w = 0.8
bar_w = group_w / n_pol
x = range(len(scenarios))

for i, pol in enumerate(POLICY_ORDER):
    sub = agg[agg["policy"] == pol].set_index("scenario").reindex(scenarios)
    offsets = [xi - group_w/2 + bar_w*i + bar_w/2 for xi in x]
    ax.bar(offsets, sub["planning_time_us_mean"], width=bar_w * 0.92,
           color=COLORS[pol], label=pol, zorder=3)

ax.yaxis.grid(True, color=GRID, linewidth=1, zorder=0)
ax.set_axisbelow(True)
for spine in ["top", "right", "left"]:
    ax.spines[spine].set_visible(False)
ax.spines["bottom"].set_color(AXIS)
ax.tick_params(axis="both", length=0)
ax.set_xticks(list(x))
ax.set_xticklabels(scenario_labels, fontsize=9)
ax.set_ylabel("Planning Time, mean (µs)")
ax.set_title("Planning Time by Scenario, grouped by Policy (mean over 4 budget levels, n=30/combo)",
              color=INK, fontsize=11, pad=14, loc="left")
leg = ax.legend(frameon=False, ncol=5, loc="upper center",
                 bbox_to_anchor=(0.5, -0.14), fontsize=9)
for text in leg.get_texts():
    text.set_color(INK_SECONDARY)

fig.tight_layout()
fig.savefig("chart_planning_time_by_scenario.png")
plt.close(fig)

print("charts written")
