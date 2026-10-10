# Keilan — behavior verification

Review preconditions and actual postconditions independently of implementation return messages. Run normal, invalid-input, partial failure and duplicate-input cases on isolated fixtures. Verify compile/save/reload and report parsing separately. Intentionally invalid fixtures/reports must fail the verifier. Preserve evidence when cleanup fails. Do not mutate the shared editor while another owner is active.
