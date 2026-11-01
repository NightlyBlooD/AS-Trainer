"""Generate checked-in relocation-compatible God Mode certificates from audited PE samples.

This engineering-only generator needs capstone and pefile. It is not part of DLL startup.
The source samples must be independently audited; the generator is not an admission policy.
"""

import argparse
import bisect
import hashlib
import json
import re
import struct
from pathlib import Path

import capstone
import pefile


class certificate_generator:
    """@brief Read audited PE functions and render immutable C++ instruction certificates."""

    # Per-operand roles audited against complete God-input control-flow gates.
    lea_roles = json.loads(r'''{
  "v132": {
    "damage": {
      "1652": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "1665": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "1722": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "2863": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "7502": {
        "kind": "rip_data",
        "width": 8,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats)."
      },
      "11398": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11419": {
        "kind": "serializer_literal",
        "width": 4,
        "reason": "Serializer type key has explicit native length4."
      },
      "11463": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11503": {
        "kind": "literal",
        "width": 4,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11524": {
        "kind": "literal",
        "width": 4,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11571": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11617": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11664": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11713": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11753": {
        "kind": "literal",
        "width": 6,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11774": {
        "kind": "literal",
        "width": 5,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11822": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11869": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11916": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11963": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "12160": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage."
      },
      "12503": {
        "kind": "rip_data",
        "width": 8,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats)."
      },
      "12542": {
        "kind": "literal",
        "width": 94,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "13145": {
        "kind": "literal",
        "width": 13,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "13554": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately."
      },
      "14725": {
        "kind": "rip_jump_table",
        "width": 16,
        "reason": "All signed table entries resolve to the same audited owning function offsets."
      },
      "16275": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "17308": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "18746": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "20736": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it."
      },
      "20750": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only."
      },
      "20757": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately."
      }
    },
    "dispatcher": {
      "756": {
        "kind": "rip_jump_table",
        "width": 148,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1348": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1362": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1508": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1678": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1692": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "1812": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2019": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2035": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2148": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2220": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "3479": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "3506": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "5782": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "5796": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "5924": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "6944": {
        "kind": "literal",
        "width": 10,
        "reason": "Literal is paired with an exact immediate length in the audited native body.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "8687": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "8701": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9051": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9065": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9188": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9758": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9772": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9892": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "9972": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "12362": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "12389": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14540": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14554": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14561": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14600": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14614": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14621": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14660": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14674": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14681": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14720": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14734": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14741": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14777": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14791": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14798": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14834": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14848": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14855": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "14877": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      }
    }
  },
  "v135": {
    "damage": {
      "1436": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "3018": {
        "kind": "inactive_skill",
        "width": 0,
        "reason": "Before cap continuation, the entire lookup is skipped when spell_id==0; dispatcher variants are never entered in God TLS scope.",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "7644": {
        "kind": "rip_data",
        "width": 8,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats)."
      },
      "10629": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10650": {
        "kind": "serializer_literal",
        "width": 4,
        "reason": "Serializer type key has explicit native length4."
      },
      "10694": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10734": {
        "kind": "literal",
        "width": 4,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10755": {
        "kind": "literal",
        "width": 4,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10801": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10848": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10895": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10944": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "10984": {
        "kind": "literal",
        "width": 6,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11005": {
        "kind": "literal",
        "width": 5,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11053": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11100": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11147": {
        "kind": "literal",
        "width": 2,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11194": {
        "kind": "literal",
        "width": 1,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "11377": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage."
      },
      "11712": {
        "kind": "rip_data",
        "width": 8,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats)."
      },
      "11752": {
        "kind": "literal",
        "width": 94,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "12321": {
        "kind": "literal",
        "width": 13,
        "reason": "Literal is paired with an exact immediate length in the audited native body."
      },
      "12708": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately."
      },
      "13788": {
        "kind": "rip_jump_table",
        "width": 16,
        "reason": "All signed table entries resolve to the same audited owning function offsets."
      },
      "15441": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "16520": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "18511": {
        "kind": "rip_address",
        "width": 28,
        "reason": "Index is SETAE boolean (two floats) or explicit <=6 bound (seven floats).",
        "gate": "spell_id=0 skips ability definition lookups; borrowed equipped spells_count=0 skips the inline spell-proc section."
      },
      "21065": {
        "kind": "code",
        "width": 0,
        "reason": "Formatting callback for no-ability panic branch; no normal spell_id==0 lookup reaches it."
      },
      "21079": {
        "kind": "diagnostic_literal",
        "width": 16,
        "reason": "Panic text formatting header and no-ability literal; tail only."
      },
      "21086": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately."
      }
    },
    "dispatcher": {
      "813": {
        "kind": "rip_jump_table",
        "width": 172,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2282": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "2611": {
        "kind": "rip_jump_table",
        "width": 20,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "4672": {
        "kind": "literal",
        "width": 10,
        "reason": "Literal is paired with an exact immediate length in the audited native body.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "6518": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "6631": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "7864": {
        "kind": "rip_jump_table",
        "width": 28,
        "reason": "All signed table entries resolve to the same audited owning function offsets.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "8951": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "8976": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "11701": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "11726": {
        "kind": "rust_stat_descriptor",
        "width": 88,
        "reason": "Five &str views followed by the immutable stat-kind qword; native heal/reward telemetry can use this on God damage.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      },
      "13925": {
        "kind": "rust_location",
        "width": 24,
        "reason": "Rust panic Location is &path,len,line32,column32; pointer relocation must be verified separately.",
        "gate": "Entire native dispatcher returns immediately while current-thread God TLS scope is active."
      }
    }
  }
}''')

    def __init__(self, path):
        """@brief Load one audited AMD64 PE without executing or modifying it.
        @param path Existing sample executable. @return Initialized read-only generator.
        """
        self.path = Path(path)
        self.data = self.path.read_bytes()
        self.pe = pefile.PE(data=self.data, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        directory = self.pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
        offset = self.pe.get_offset_from_rva(directory.VirtualAddress)
        self.functions = [struct.unpack_from("<III", self.data, offset + index)[:2]
                          for index in range(0, directory.Size, 12)]
        self.starts = [item[0] for item in self.functions]
        self.decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.decoder.detail = True
        self.text = next(section for section in self.pe.sections if section.Name.rstrip(b"\0") == b".text")
        self.text_data = self.data[self.text.PointerToRawData:self.text.PointerToRawData + self.text.SizeOfRawData]
        self.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        self.imports = {item.address - self.base: module.dll.decode("ascii") + "!" +
                        (item.name.decode("ascii") if item.name else "#" + str(item.ordinal))
                        for module in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []) for item in module.imports}
        self.anchor_cache = {}
        self.helpers = {}
        self.helper_order = []
        self.helper_depth = 0
        self.family_name = ""

    def read(self, rva, size):
        """@brief Copy a file-backed RVA range from the audited sample.
        @param rva Image-relative first byte. @param size Width. @return Immutable bytes.
        """
        offset = self.pe.get_offset_from_rva(rva)
        return self.data[offset:offset + size]

    def bounds(self, rva):
        """@brief Find exact unwind bounds without inventing leaf-function extents.
        @param rva Candidate entry. @return Entry/end pair or None when unwind metadata is absent.
        """
        index = bisect.bisect_right(self.starts, rva) - 1
        if index >= 0 and rva < self.functions[index][1]:
            return self.functions[index]
        return None

    def executable(self, rva):
        """@brief Read the PE section access kind of one certified reference target.
        @param rva File-backed target. @return True for code, false for immutable data.
        """
        for section in self.pe.sections:
            if section.VirtualAddress <= rva < section.VirtualAddress + section.SizeOfRawData:
                if not section.Characteristics & 0x40000000 or section.Characteristics & 0x80000000:
                    raise ValueError(f"reference target has mutable/unreadable section: {rva:x}")
                return bool(section.Characteristics & 0x20000000)
        raise ValueError(f"reference target is outside file-backed sections: {rva:x}")

    def entry_pattern(self, rva):
        """@brief Build a discriminating helper anchor or exact imported-symbol identity.
        @param rva Certified direct-call target. @return Pattern text and exact unwind-entry flag.
        """
        original_rva = rva
        if rva in self.anchor_cache:
            return self.anchor_cache[rva]
        visited = set()
        for _ in range(8):
            if rva in visited:
                raise ValueError("cyclic callee forwarder")
            visited.add(rva)
            data = self.read(rva, 6)
            if data[0] == 0xe9:
                rva = rva + 5 + struct.unpack_from("<i", data, 1)[0]
                continue
            if data[:2] == b"\xff\x25":
                slot = rva + 6 + struct.unpack_from("<i", data, 2)[0]
                if slot not in self.imports:
                    raise ValueError(f"callee thunk does not identify an import: {rva:x}")
                result = ("", False, self.imports[slot], None)
                self.anchor_cache[original_rva] = result
                return result
            break
        else:
            raise ValueError("callee forwarder depth exceeded")
        if self.read(rva, 1) == b"\xc3":
            result = ("C3", False, "", None)
            self.anchor_cache[original_rva] = result
            return result
        bounds = self.bounds(rva)
        size = min(1024, bounds[1] - rva) if bounds is not None else 1024
        decoded = self.decoder.disasm(self.read(rva, size), self.base + rva)
        raw = bytearray()
        mask = []
        selected = None
        for index, instruction in enumerate(decoded):
            position = len(raw)
            raw.extend(instruction.bytes)
            mask.extend([False] * instruction.size)
            if any(operand.type == capstone.x86.X86_OP_MEM and
                   operand.mem.base == capstone.x86.X86_REG_RIP for operand in instruction.operands):
                mask[position + instruction.disp_offset:position + instruction.disp_offset + 4] = [True] * 4
            if instruction.group(capstone.CS_GRP_CALL):
                if instruction.imm_size and not (bounds is not None and bounds[0] <=
                    instruction.operands[0].imm - self.base < bounds[1]):
                    mask[position + instruction.imm_offset:position + instruction.imm_offset + instruction.imm_size] = [True] * instruction.imm_size
            complete = instruction.mnemonic in ("ret", "ud2") and (bounds is None or instruction.address + instruction.size == self.base + bounds[1])
            if index >= 31 or complete:
                expression = b"".join(b"." if wildcard else re.escape(bytes([value])) for value, wildcard in zip(raw, mask))
                found = list(re.finditer(expression, self.text_data, re.DOTALL))[:2]
                if len(found) == 1:
                    selected = self.pattern(raw, mask)
                    break
            if complete:
                break
        if selected is None:
            if bounds is None or bounds[0] != rva or bounds[1] - bounds[0] > 4096 or self.helper_depth >= 4:
                raise ValueError(f"native callee lacks a discriminating entry anchor: {rva:x}")
            if rva not in self.helpers:
                name = f"{self.family_name}_helper_{len(self.helpers)}"
                self.helpers[rva] = {"name": name, "certificate": None}
                self.helper_depth += 1
                try:
                    self.helpers[rva]["certificate"] = self.function(rva, "helper")
                finally:
                    self.helper_depth -= 1
                self.helper_order.append(rva)
            helper = self.helpers[rva]
            if helper["certificate"] is None:
                raise ValueError("cyclic ambiguous wrapper dependency")
            result = (helper["certificate"]["entry_pattern"], True, "", helper["name"])
            self.anchor_cache[original_rva] = result
            return result
        result = (selected, bounds is not None and bounds[0] == rva, "", None)
        self.anchor_cache[original_rva] = result
        return result

    @staticmethod
    def pattern(raw, mask):
        """@brief Render byte pairs and address-only wildcards.
        @param raw Instruction bytes. @param mask Per-byte relocation flags. @return IDA pattern.
        """
        return " ".join("??" if wildcard else f"{value:02X}" for value, wildcard in zip(raw, mask))

    def function(self, rva, function_name):
        """@brief Certify a complete audited function while preserving semantic bytes.
        @param rva Proven unwind entry. @return Shape digest, references, entry signature and code.
        Only RIP disp32 and external E8 rel32 are masked. Internal branches and self calls stay exact.
        """
        bounds = self.bounds(rva)
        if bounds is None or bounds[0] != rva:
            raise ValueError(f"function has no exact unwind entry: {rva:x}")
        begin, end = bounds
        raw = self.read(begin, end - begin)
        code = list(self.decoder.disasm(raw, self.base + begin))
        if sum(instruction.size for instruction in code) != len(raw):
            raise ValueError("incomplete instruction decoding")
        normalized = bytearray(raw)
        references = []
        for instruction in code:
            position = instruction.address - self.base - begin
            rip = [operand for operand in instruction.operands if operand.type == capstone.x86.X86_OP_MEM and
                   operand.mem.base == capstone.x86.X86_REG_RIP]
            if rip:
                offset = position + instruction.disp_offset
                target = instruction.address + instruction.size + rip[0].mem.disp - self.base
                if begin <= target < end:
                    continue
                normalized[offset:offset + 4] = bytes(4)
                address_only = instruction.mnemonic == "lea"
                imported = ""
                if instruction.group(capstone.CS_GRP_CALL) or instruction.group(capstone.CS_GRP_JUMP):
                    if target not in self.imports:
                        raise ValueError("indirect RIP control transfer has no exact import identity")
                    imported = self.imports[target]
                width = 0 if address_only else rip[0].size
                if width > 16:
                    raise ValueError("unsupported scalar/packed constant width")
                reference = {"operand_offset": offset, "instruction_end": position + instruction.size,
                                   "kind": "rip_import" if imported else "rip_address" if address_only else "rip_data",
                                   "width": 0 if imported else width,
                                   "expected_value": "" if address_only or imported else self.read(target, width).hex(),
                                   "target_pattern": "", "executable_target": self.executable(target),
                                   "require_function_entry": False, "target_import": imported, "target_digest": "",
                                   "target_certificate": None, "audit_target": target, "role": "scalar constant"}
                if address_only:
                    self.classify_address(reference, function_name, begin)
                references.append(reference)
            if instruction.group(capstone.CS_GRP_CALL) or instruction.group(capstone.CS_GRP_JUMP):
                immediate = [operand for operand in instruction.operands if operand.type == capstone.x86.X86_OP_IMM]
                if not immediate:
                    continue
                target = immediate[0].imm - self.base
                if begin <= target < end:
                    continue
                if instruction.mnemonic != "call" or instruction.imm_size != 4:
                    raise ValueError("unhandled external branch")
                offset = position + instruction.imm_offset
                normalized[offset:offset + 4] = bytes(4)
                pattern, unwind_entry, imported, helper = self.entry_pattern(target)
                references.append({"operand_offset": offset, "instruction_end": position + instruction.size,
                                   "kind": "relative_call", "width": 0, "expected_value": "",
                                   "target_pattern": pattern, "executable_target": True,
                                   "require_function_entry": unwind_entry, "target_import": imported,
                                   "target_digest": "", "target_certificate": helper, "audit_target": target,
                                   "role": "complete wrapper" if helper else "specific native/import callee"})
        final_anchor_instruction = code[min(31, len(code) - 1)]
        anchor_size = final_anchor_instruction.address + final_anchor_instruction.size - self.base - begin
        mask = [False] * anchor_size
        for reference in references:
            offset = reference["operand_offset"]
            if offset + 4 <= anchor_size:
                mask[offset:offset + 4] = [True] * 4
        entry_pattern = self.pattern(raw[:anchor_size], mask)
        return {"rva": rva, "size": end - begin, "entry_pattern": entry_pattern,
                "sha256": hashlib.sha256(normalized).hexdigest(), "references": references}

    def classify_address(self, reference, function_name, function_rva):
        """@brief Assign an audited semantic guard to every address-only RIP operand.
        @param reference Decoded LEA metadata. @param function_name Damage/dispatcher/helper role.
        @param function_rva Certified containing entry. @return None; rejects unclassified dependencies.
        """
        roles = self.lea_roles.get("v135" if self.family_name == "modern" else "v132", {})
        role = roles.get(function_name, {}).get(str(reference["operand_offset"]))
        if role is None:
            raise ValueError(f"unclassified {function_name} LEA {reference['operand_offset']} in {function_rva:x}")
        target = reference["audit_target"]
        kind = role["kind"]
        reference["role"] = role.get("gate", role["reason"])
        if kind == "rip_jump_table":
            width = role["width"]
            data = self.read(target, width)
            relative = [target + value[0] - function_rva for value in struct.iter_unpack("<i", data)]
            size = self.bounds(function_rva)[1] - function_rva
            if any(value < 0 or value >= size for value in relative):
                raise ValueError("jump table does not stay in the certified function")
            reference["kind"] = "rip_jump_table"
            reference["width"] = width
            reference["target_digest"] = hashlib.sha256(struct.pack("<" + "I" * len(relative), *relative)).hexdigest()
        elif function_name == "dispatcher" or kind in ("inactive_skill", "rust_location", "diagnostic_literal", "code"):
            reference["kind"] = "rip_inactive"
            reference["width"] = max(role["width"], 16)
            if function_name == "dispatcher":
                reference["role"] = "God TLS scope returns before dispatcher body; ordinary events delegate unchanged."
        elif kind == "rust_stat_descriptor":
            width = 88
            data = bytearray(self.read(target, width))
            semantic = bytearray(data)
            key = b""
            for index in range(5):
                pointer, length = struct.unpack_from("<QQ", data, index * 16)
                if not 0 < length <= 512:
                    raise ValueError("malformed stat descriptor string")
                semantic[index * 16:index * 16 + 8] = bytes(8)
                if index:
                    semantic[index * 16 + 8:index * 16 + 16] = bytes(8)
                else:
                    key = self.read(pointer - self.base, length)
            reference["kind"] = "rip_string_table"
            reference["width"] = width
            reference["target_digest"] = hashlib.sha256(semantic + key).hexdigest()
            reference["role"] = "Active stat lookup: exact key/tag; five bounded Rust strings, cosmetic views may vary."
        elif kind in ("rip_data", "rip_address", "literal", "serializer_literal"):
            width = role["width"]
            data = self.read(target, width)
            reference["width"] = width
            if width <= 16:
                reference["kind"] = "rip_data"
                reference["expected_value"] = data.hex()
            else:
                reference["kind"] = "rip_address"
                reference["target_pattern"] = " ".join(f"{value:02X}" for value in data)
        else:
            raise ValueError(f"unsupported LEA role {kind}")

    @staticmethod
    def render_function(name, certificate):
        """@brief Render immutable function reference metadata and its normalized SHA contract.
        @param name C++ identifier. @param certificate Audited decoded function. @return C++ fragment.
        """
        lines = [f"static constexpr relocation_rule {name}_references[] = {{"]
        for reference in certificate["references"]:
            lines.append("    // " + reference["role"].replace("\n", " "))
            expected = "{" + ", ".join(f"0x{value:02x}" for value in bytes.fromhex(reference["expected_value"])) + "}"
            pattern = json.dumps(reference["target_pattern"])
            lines.append("    {" + f'{reference["operand_offset"]}, {reference["instruction_end"]}, '
                         f'reference_kind::{reference["kind"]}, {reference["width"]}, {expected}, {pattern}, '
                         f'{str(reference["executable_target"]).lower()}, {str(reference["require_function_entry"]).lower()}, '
                         f'{json.dumps(reference["target_import"])}, {json.dumps(reference["target_digest"])}, '
                         + ("&" + reference["target_certificate"] if reference["target_certificate"] else "nullptr") + "},")
        lines += ["};", f"static constexpr function_certificate {name}{{",
                  f'    "{certificate["entry_pattern"]}",', f'    {certificate["size"]},',
                  f'    "{certificate["sha256"]}", {name}_references', "};"]
        return "\n".join(lines)

    @staticmethod
    def main():
        """@brief Generate two independently audited machine-code-family certificates.
        @return None; writes the requested checked-in header, without changing any executable.
        """
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--v132", type=Path, required=True)
        parser.add_argument("--v135", type=Path, required=True)
        parser.add_argument("--output", type=Path, required=True)
        arguments = parser.parse_args()
        profiles = [
            ("v132", arguments.v132, 0x5ce6440, 0x5d2c0e0, 0x5cdbd50, 0x5b8, 0x1f8, 0x1b6, 0xec3, 0x388),
            ("modern", arguments.v135, 0x5cdfeb0, 0x5d23360, 0x5cd6600, 0x640, 0x200, 0x236, 0xf4b, 0x408),
        ]
        output = ["// Generated by scripts/generate_god_mode_certificates.py from independently audited samples.",
                  "// Included only inside god_mode_bindings::resolve_file; not an executable-hash allowlist.",
                  "// Whole-function fingerprints preserve layout, ABI, immediates and internal control flow."]
        families = []
        for name, path, damage_rva, dispatcher_rva, getter_rva, enemies, stride, source, phase, spells in profiles:
            generator = certificate_generator(path)
            generator.family_name = name
            damage = generator.function(damage_rva, "damage")
            dispatcher = generator.function(dispatcher_rva, "dispatcher")
            for helper_rva in generator.helper_order:
                helper = generator.helpers[helper_rva]
                output.extend(["", generator.render_function(helper["name"], helper["certificate"])])
            output.extend(["", generator.render_function(name + "_damage", damage), "",
                           generator.render_function(name + "_dispatcher", dispatcher)])
            dispatcher_operands = [entry["operand_offset"] for entry in damage["references"]
                                   if entry["kind"] == "relative_call" and entry["audit_target"] == dispatcher_rva]
            getter_operands = [entry["operand_offset"] for entry in damage["references"]
                               if entry["kind"] == "relative_call" and entry["audit_target"] == getter_rva]
            if len(dispatcher_operands) != 2 or len(getter_operands) != 1:
                raise ValueError("required linked dispatcher/getter call sites changed")
            families.append("    {" + f"{name}_damage, {name}_dispatcher, 0x{enemies:x}, 0x{stride:x}, "
                            f"0x{source:x}, 0x{phase:x}, 0x{spells:x}, " + "{" +
                            ", ".join(str(value) for value in dispatcher_operands) + "}, " +
                            str(getter_operands[0]) + "},")
        output += ["", "static constexpr family_certificate families[] = {", *families, "};", ""]
        arguments.output.write_text("\n".join(output), encoding="utf-8")


if __name__ == "__main__":
    certificate_generator.main()
