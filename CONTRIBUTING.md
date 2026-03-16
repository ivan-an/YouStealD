> 🌐 Language: [English](CONTRIBUTING.md) | [Русский](CONTRIBUTING.ru.md)

---

# 👋 Welcome to YouStealD Contributions!

Hey there! Thanks for wanting to help make YouStealD better. 🎉
This is a pet project born from the desire to download YouTube videos without wrestling with command-line flags — and it's grown into something I'm proud of.

Whether you're fixing a typo, adding a feature, or just reporting a weird bug — every bit helps.

---

## 🐞 Found a Bug?

Before opening an issue, please:
1. Check [existing issues](https://github.com/ivan-an/YouStealD/issues) — maybe it's already known.
2. Try the [latest release](https://github.com/ivan-an/YouStealD/releases) — your bug might already be fixed.

If it's still there, tell me:
- **What OS + Qt version** you're using (e.g., "Windows 11, Qt 6.6 MSVC")
- **What you did** (steps to reproduce)
- **What happened** vs **what you expected**
- **Logs help a lot** — enable the debug panel in Settings or run from console
- **yt-dlp version** (`yt-dlp --version`) — sometimes the issue is upstream

> 💡 Pro tip: If the app crashes on startup, try running `YouStealD.exe --debug` from terminal — it might print a useful error.

---

## 💡 Got an Idea?

Cool! Before suggesting:
- Search [Issues](https://github.com/ivan-an/YouStealD/issues) and [Discussions](https://github.com/ivan-an/YouStealD/discussions) — maybe someone already asked for this.
- Think about the *why*: "I want to download Shorts" is good, but "I want to archive my favorite creator's content automatically" is even better — it helps me understand the real need.

Open an issue with the `enhancement` label and describe:
- What problem this solves
- How you imagine it working (mockups welcome!)
- Any edge cases you've thought about

---

## 🛠️ Want to Code?

Awesome! Here's how to make your contribution smooth:

### 1. Fork & Branch
```bash
git clone https://github.com/your-username/YouStealD.git
cd YouStealD
git checkout -b feat/cool-new-thing  # or fix/bug-description
