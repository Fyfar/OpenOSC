"""The CMSIS-DSP v1.14.4 subset OpenOSC links (vendor/cmsis_dsp): used by the firmware build. Only the tables of a 2048-point real FFT are compiled."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "vendor/cmsis_dsp/Include"
SOURCES = [ROOT / "vendor/cmsis_dsp/Source" / name for name in (
    "TransformFunctions/arm_rfft_fast_f32.c", "TransformFunctions/arm_rfft_fast_init_f32.c",
    "TransformFunctions/arm_cfft_f32.c", "TransformFunctions/arm_cfft_init_f32.c",
    "TransformFunctions/arm_cfft_radix8_f32.c", "TransformFunctions/arm_bitreversal2.c",
    "ComplexMathFunctions/arm_cmplx_mag_f32.c",
    "CommonTables/arm_common_tables.c", "CommonTables/arm_const_structs.c")]
# RFFT 2048 = CFFT 1024 + the 2048-point real twiddles; nothing else from the 20 MB of tables.
DEFINES = ["ARM_DSP_CONFIG_TABLES", "ARM_FFT_ALLOW_TABLES", "ARM_TABLE_TWIDDLECOEF_F32_1024",
           "ARM_TABLE_BITREVIDX_FLT_1024", "ARM_TABLE_TWIDDLECOEF_RFFT_F32_2048"]
