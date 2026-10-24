"""UE 5.8 public editor transaction scope, never an atomic rollback promise."""

import copy
import json
import unreal


_QUARANTINED_SCOPE = None


def quarantined_scope():
    # Pure Python metadata. Reconciliation/recovery never runs automatically.
    return copy.deepcopy(_QUARANTINED_SCOPE)


class _BatchTransactionError(RuntimeError):
    code = "batch_transaction_quarantined"


class guarded_transaction:
    """Exact synchronous Batch scope; native checks precede every continuation.

    No object/transaction destructor performs an implicit close. The caller
    reports durable observer loss explicitly and preserves the unresolved scope.
    """
    def __init__(self, operation_id, observation):
        self.operation_id, self.observation = operation_id, observation
        self.scope_id = None
        self.opened = self.closed = self.lost = self.observer_lost = False

    def _lose(self, reason):
        global _QUARANTINED_SCOPE
        self.lost = True
        self.observation.update(scope_quarantined=True, scope_open=None,
            scope_outcome_unknown=True, grouping_preserved=False,
            full_batch_transaction_contract_complete=False, close_retry_performed=False,
            reason=str(reason)[:1000])
        _QUARANTINED_SCOPE = dict(operation_id=self.operation_id, scope_id=self.scope_id,
            observation=copy.deepcopy(self.observation), automatic_retry=False)

    def __enter__(self):
        if _QUARANTINED_SCOPE is not None:
            raise _BatchTransactionError("A prior batch transaction requires reconciliation; no new native call")
        self.observation.update(transaction_scope="guarded_native_synchronous_batch",
            grouping_preserved=False, full_batch_transaction_contract_complete=False,
            checks=0, atomic_rollback=False, across_frames_supported=False)
        try:
            receipt = json.loads(unreal.CubelessPythonExecutionLibrary.begin_batch_transaction(self.operation_id))
            self.observation["begin"] = receipt
            if not receipt.get("success"):
                if receipt.get("scope_outcome_unknown") or receipt.get("scope_quarantined"):
                    self._lose(receipt.get("error", "Native begin outcome unknown"))
                raise _BatchTransactionError(receipt.get("error", "Native batch transaction begin refused"))
            self.scope_id = receipt["scope_id"]
            self.opened = True
            self.observation.update(batch_scope_entered=True, scope_open=True, scope_quarantined=False,
                scope_outcome_unknown=False, scope_id=self.scope_id)
            return self
        except _BatchTransactionError:
            raise
        except Exception as error:
            self._lose("Native Begin return/decoding failed; no End or automatic replay: " + str(error))
            raise _BatchTransactionError(self.observation["reason"]) from error

    def check(self):
        if self.closed:
            return
        if self.lost or self.observer_lost or not self.opened:
            raise _BatchTransactionError("Batch transaction continuation is no longer certified")
        self.observation["checks"] += 1
        try:
            receipt = json.loads(unreal.CubelessPythonExecutionLibrary.observe_batch_transaction(self.scope_id, "check"))
            if not receipt.get("success"):
                self.observation["first_failed_check"] = receipt
                raise _BatchTransactionError(receipt.get("error", "Native scope changed"))
        except Exception as error:
            self._lose(str(error))
            raise _BatchTransactionError(self.observation["reason"]) from error

    def observer_failed(self):
        # After journal/observer loss there is no further native/Editor query,
        # End, destructor action or fallback close in this request.
        self.observer_lost = True
        self._lose("Batch durable observer was lost; no later native scope call or End")
        self.observation["close_skipped_observer_lost"] = True

    def __exit__(self, error_type, error, traceback):
        if self.lost or self.observer_lost:
            self.observation["close_skipped_unknown"] = True
            if error_type is None:
                raise _BatchTransactionError(self.observation.get("reason", "Scope outcome unknown"))
            return False
        try:
            # Native tests the actual owned lease again; child success/unknown
            # is deliberately not the close admission condition.
            receipt = json.loads(unreal.CubelessPythonExecutionLibrary.observe_batch_transaction(self.scope_id, "end"))
            self.observation["end"] = receipt
            if not receipt.get("success") or not receipt.get("scope_closed"):
                self._lose(receipt.get("error", "Native End was not certified"))
            else:
                self.closed = True
                self.observation.update(scope_open=False, scope_outcome_unknown=False,
                    scope_closed=True, grouping_preserved=True,
                    full_batch_transaction_contract_complete=False,
                    original_single_undo_grouping="synchronous_native_scope_restored_usage_validation_pending")
        except Exception as close_error:
            self._lose("Native End return/decoding failed; no second End: " + str(close_error))
        if self.lost and error_type is None:
            raise _BatchTransactionError(self.observation["reason"])
        return False


def transaction():
    # PyEditor.cpp: BeginTransaction on enter, EndTransaction on exit. An
    # exception does not undo work; persisted saves are outside undo recovery.
    return unreal.ScopedEditorTransaction("Cubeless bounded batch")
