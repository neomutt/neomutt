/**
 * Provides custom sanitizers for NeoMutt's Buffer pool operations.
 *
 * This module should be imported by taint tracking queries to recognize
 * when Buffers are sanitized by buf_reset(), memset(), or buf_pool_release().
 */

import cpp
import semmle.code.cpp.dataflow.new.TaintTracking

/**
 * A call to a function that clears a Buffer's memory.
 */
class BufferClearingFunction extends FunctionCall {
  BufferClearingFunction() {
    this.getTarget().getName() in ["buf_reset", "buf_pool_release", "memset"]
  }
}

/**
 * A sanitizer node for Buffer pool operations.
 * This marks data as clean after certain function calls.
 */
class BufferPoolSanitizer extends DataFlow::Node {
  BufferPoolSanitizer() {
    // After buf_reset(buf), the buffer is cleared
    exists(FunctionCall call |
      call.getTarget().getName() = "buf_reset" and
      this.asExpr() = call.getArgument(0)
    )
    or
    // After buf_pool_release(&buf), the buffer is cleared (calls buf_reset internally)
    // buf_pool_release takes Buffer** and internally dereferences to call buf_reset(*ptr)
    // The sanitization applies to the buffer being released, not the pointer itself
    exists(FunctionCall call |
      call.getTarget().getName() = "buf_pool_release" and
      (
        this.asExpr() = call.getArgument(0)
        or
        // Also sanitize pointer dereference for dataflow tracking
        this.asExpr().(PointerDereferenceExpr).getOperand() = call.getArgument(0)
      )
    )
    or
    // After memset(ptr, 0, size), the memory is cleared
    exists(FunctionCall call |
      call.getTarget().getName() = "memset" and
      call.getArgument(1).getValue().toInt() = 0 and
      this.asExpr() = call.getArgument(0)
    )
    or
    // buf_pool_get() returns a clean buffer (previously sanitized by buf_pool_release)
    // All buffers in the pool have been cleared by buf_reset before being returned to pool
    exists(FunctionCall call |
      call.getTarget().getName() = "buf_pool_get" and
      this.asExpr() = call
    )
  }
}
