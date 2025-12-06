/**
 * Repository-local CodeQL model: treat buf_reset() as taint sanitizers so
 * CodeQL will not treat cleaned pool memory as uncontrolled tainted data.
 */
import cpp
import semmle.cpp.dataflow.TaintTracking as Taint

class NeoMuttBufReset extends Taint::Sanitizer {
  NeoMuttBufReset() { this.getName() = "NeoMuttBufReset" }

  override predicate isCall(DataFlow::CallNode call) {
    exists(Function f |
      call.getTarget() = f and
      (
        f.getName() = "buf_reset" or
        f.getName() = "buf_pool_release"
      )
    )
  }
}
