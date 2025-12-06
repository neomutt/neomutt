/**
 * Repository-local CodeQL model: treat buf_reset(buf) as a taint sanitizer
 * for the memory reachable from buf->data.
 *
 * What this does
 * - Marks calls to `buf_reset(...)` as sanitizers for the taint-tracking
 *   engine used by CodeQL for C/C++.
 * - More precisely, it tells the taint tracker that data flows that
 *   originate from the field named `data` of the first argument to
 *   `buf_reset` are sanitized by the call.
 */

import cpp
import semmle.cpp.dataflow.TaintTracking as Taint
import DataFlow

class NeoMuttBufResetSanitizer extends Taint::Sanitizer {
  NeoMuttBufResetSanitizer() { this.getName() = "NeoMuttBufResetSanitizer" }

  override predicate isCall(DataFlow::CallNode call) {
    exists(Function f |
      call.getTarget() = f and
      f.getName() = "buf_reset"
    )
  }

  /**
   * Precisely declare what this sanitizer sanitizes:
   * - If the tainted source is a field access named "data" on the
   *   expression passed as the first argument to buf_reset(...),
   *   then the call sanitizes that source.
   *
   * This keeps the model narrow and avoids classifying unrelated uses
   * of buf_reset as sanitizing other, unrelated memory.
   */
  override predicate sanitizes(DataFlow::Node source, DataFlow::Node sink) {
    exists(DataFlow::CallNode call |
      isCall(call) and
      // source must be an expression that is a FieldAccess (e.g. buf->data or buf.data)
      source.asExpr() instanceof FieldAccess and
      let fa = source.asExpr().(FieldAccess) |
        // the field name we care about
        fa.getField().getName() = "data" and
        // the qualifier of the field access (the 'buf' in buf->data) must be
        // the same expression as the first argument passed to buf_reset(...)
        call.getArgument(0).getAExpr() = fa.getQualifier()
    )
  }
}
