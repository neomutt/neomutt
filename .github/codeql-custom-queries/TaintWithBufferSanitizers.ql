/**
 * @name Taint tracking with Buffer pool sanitizers
 * @description Detects taint flow with custom sanitizers for NeoMutt Buffer pool
 * @kind path-problem
 * @problem.severity warning
 * @id neomutt/taint-with-buffer-sanitizers
 * @tags security
 *       external/cwe/cwe-079
 *       external/cwe/cwe-089
 */

import cpp
import semmle.code.cpp.dataflow.new.TaintTracking
import CustomSanitizers

/**
 * Configuration for taint tracking that includes Buffer pool sanitizers.
 */
private module Config implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    // Sources: user input, file reads, network reads, config parsing
    exists(FunctionCall call |
      call.getTarget().getName() in [
        // Standard C input functions
        "fgets", "fgetc", "getc", "getchar", "gets", "scanf", "fscanf",
        "read", "recv", "recvfrom", "recvmsg",
        // NeoMutt specific input functions
        "mutt_file_read_line", "mutt_socket_readln"
      ] and
      (source.asExpr() = call or source.asExpr() = call.getAnArgument())
    )
  }

  predicate isSink(DataFlow::Node sink) {
    // Sinks: command execution, SQL queries, format strings
    exists(FunctionCall call |
      call.getTarget().getName() in [
        // Command execution
        "system", "popen", "execl", "execlp", "execle",
        "execv", "execvp", "execve", "execvpe",
        // Other potentially dangerous functions
        "eval"
      ] and
      sink.asExpr() = call.getAnArgument()
    )
    or
    // Format string vulnerabilities
    exists(FunctionCall call |
      call.getTarget().getName() in ["printf", "fprintf", "sprintf", "snprintf"] and
      sink.asExpr() = call.getArgument(0)
    )
  }

  predicate isBarrier(DataFlow::Node node) {
    // Use our custom Buffer pool sanitizers
    node instanceof BufferPoolSanitizer
  }
}

private module Flow = TaintTracking::Global<Config>;
import Flow::PathGraph

from Flow::PathNode source, Flow::PathNode sink
where Flow::flowPath(source, sink)
select sink.getNode(), source, sink,
  "Taint flow from $@ to $@.",
  source.getNode(), "user input",
  sink.getNode(), "dangerous sink"
