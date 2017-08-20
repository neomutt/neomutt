;;; Copyright (C) 2017 ng0 <ng0@infotropique.org>
;;;
;;; This program is free software: you can redistribute it and/or modify it under
;;; the terms of the GNU General Public License as published by the Free Software
;;; Foundation, either version 2 of the License, or (at your option) any later
;;; version.
;;;
;;; This program is distributed in the hope that it will be useful, but WITHOUT
;;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
;;; FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
;;; details.
;;;
;;; You should have received a copy of the GNU General Public License along with
;;; this program.  If not, see <http://www.gnu.org/licenses/>.
;;;
;;; How to use this file.
;;; Building neomutt from within the git checkout, skipping the hash check:
;;; cd contrib
;;; guix build -f guix-neomutt.scm

(use-modules
 (ice-9 popen)
 (ice-9 match)
 (ice-9 rdelim)
 (guix packages)
 (guix build-system gnu)
 (guix gexp)
 ((guix build utils) #:select (with-directory-excursion))
 (gnu packages)
 (gnu packages base)
 (gnu packages autotools)
 (gnu packages mail)
 (gnu packages gettext))

(define %source-dir (canonicalize-path ".."))

(define-public neomutt-git
  (package
    (inherit neomutt)
    (name "neomutt-git")
    (version (string-append (package-version neomutt) "-git"))
    (source
     (local-file %source-dir
                 #:recursive? #t))
    (native-inputs
      `(("gettext-minimal" ,gettext-minimal)
        ,@(package-native-inputs neomutt)))))

neomutt-git
