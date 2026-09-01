# RF validation captures

This directory is reserved for captures collected during the BETA hardware-validation phase.

For each validated Oregon profile, store at least:

- logic-analyzer or oscilloscope capture of the ESP32 TX DATA line;
- companion LILYGO raw frame / diagnostic output;
- notes about RF module supply voltage and antenna;
- WMR88/WMR200 reception result;
- firmware commit SHA and Web UI settings used for the test.

Suggested naming convention:

```text
YYYY-MM-DD_THGR810_F824_CH1_<short-commit>.*
YYYY-MM-DD_UVN800_D874_<short-commit>.*
```

Do not mark a sensor profile as validated only because the companion gateway decodes it. Acceptance requires an original Oregon console test as described in `docs/TEST_PLAN.md`.
