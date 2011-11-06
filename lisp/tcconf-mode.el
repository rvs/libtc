; Copyright (C) 2003  Michael Ahlberg, Måns Rullgård

; Permission is hereby granted, free of charge, to any person
; obtaining a copy of this software and associated documentation
; files (the "Software"), to deal in the Software without
; restriction, including without limitation the rights to use, copy,
; modify, merge, publish, distribute, sublicense, and/or sell copies
; of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:

; The above copyright notice and this permission notice shall be
; included in all copies or substantial portions of the Software.

; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
; EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
; MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
; NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
; HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
; WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
; DEALINGS IN THE SOFTWARE.

(require 'font-lock)

(defgroup tcconf nil
  "Editing libtc configuration files."
  :group 'languages)

(defcustom tcconf-indent-offset 8
  "Amount of indentation."
  :type 'integer
  :group 'tcconf)

(defvar tcconf-syntax-table nil
  "Syntax table for tcconf configuration files.")

(defvar tcconf-mode-map nil
  "Keymap for tcconf-mode.")

(defface tcconf-section-face
  '((t (:foreground "magenta4")))
  "Face used to hightlight section names."
  :group 'tcconf)

(defface tcconf-merge-face
  '((t (:foreground "steelblue")))
  "Face used to highlight merged sections."
  :group 'tcconf)

(defface tcconf-key-face
  '((t (:foreground "red4")))
  "Face used to highlight configuration keys."
  :group 'tcconf)

(defun tcconf-build-syntax-table (table)
  (modify-syntax-entry ?_  "_"   table)
  (modify-syntax-entry ?.  "_"   table)
  (modify-syntax-entry ?/  "_"   table)
  (modify-syntax-entry ?\" "\""  table)
  (modify-syntax-entry ?\' "\""  table)
  (modify-syntax-entry ?#  "< b" table)
  (modify-syntax-entry ?\n "> b" table))

(if tcconf-syntax-table nil
  (setq tcconf-syntax-table (make-syntax-table))
  (tcconf-build-syntax-table tcconf-syntax-table))

(if tcconf-mode-map nil
  (setq tcconf-mode-map (make-sparse-keymap))
  (define-key tcconf-mode-map "\C-c\C-c" 'comment-region)
  (define-key tcconf-mode-map "\C-c\C-s" 'tcconf-comment-section)
  (define-key tcconf-mode-map [(control meta h)]'tcconf-mark-section)
  (define-key tcconf-mode-map "\C-c\C-w" 'tcconf-kill-section)
  (define-key tcconf-mode-map [tab] 'indent-for-tab-command))

(defun tcconf-match-string-ref ()
  (let ((bounds (text-property-bounds (1- (point)) 'font-lock)))
    (cond ((char= (char-before (car bounds)) ?\")
	   'font-lock-reference-face)
	  ('font-lock-string-face))))

(defvar tcconf-font-lock-keywords
  '(t ("\\(^\\|;\\)\\s-*\\(\\(\\sw\\|\\s_\\)+\\)\\s-*[:{[]"
       (2 tcconf-section-face nil t))
      (":\\s-*\\(\\(\\sw\\|\\s_\\)+\\)\\s-*[:{[]" (1 tcconf-merge-face))
      ("\\(^\\|;\\|\\s(\\)\\s-*\\(\\(\\sw\\|\\s_\\)+\\)"
       (2 tcconf-key-face nil t))
      ("[^\\\\]\\(\\$([^\)]*)\\)" (1 (tcconf-match-string-ref) t)))
  "Font Lock mode keywords for tcconf-mode.")

(defun tcconf-mode ()
  "Major mode for editing configuration files loadable by libtc."
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table tcconf-syntax-table)
  (setq major-mode 'tcconf-mode
	mode-name "TCconf")
  (use-local-map tcconf-mode-map)
  (make-local-variable 'comment-start)
  (make-local-variable 'comment-end)
  (make-local-variable 'font-lock-defaults)
  (make-local-variable 'indent-line-function)
  (setq comment-start "# "
	comment-end ""
	font-lock-defaults '(tcconf-font-lock-keywords)
	indent-line-function 'tcconf-indent-line)
  (redraw-modeline))

(put 'tcconf-mode 'font-lock-defaults
     '(tcconf-font-lock-keywords nil nil nil nil))

(defun tcconf-beginning-of-section (&optional count)
  "Move to the beginning of the current section.
With numeric prefix arg, move out that many sections."
  (interactive "p")
  (setq count (or count 1))
  (while (>= (setq count (1- count)) 0)
    (while (and (re-search-backward "\\s(\\|\\s)" nil 1)
		(looking-at "\\s)"))
      (forward-char)
      (backward-sexp))))

(defun tcconf-indent-line ()
  "Indent the current line."
  (interactive)
  (or (and (not (eobp)) (char= (char-after (point-at-bol)) ?#))
      (let (ind top)
	(save-excursion
	  (save-excursion
	    (back-to-indentation)
	    (tcconf-beginning-of-section)
	    (setq ind (current-indentation)
		  top (bobp)))
	  (back-to-indentation)
	  (and (not (or top (looking-at "\\s)")))
	       (setq ind (+ ind tcconf-indent-offset)))
	  (indent-line-to ind))
	(if (< (current-column) ind)
	    (back-to-indentation)))))

(defun tcconf-mark-section (&optional arg dont-activate)
  "Mark the current (innermost) section.
With numeric prefix arg ARG, mark ARG sections outward."
  (interactive "P")
  (tcconf-beginning-of-section (cond ((numberp arg) arg) (1)))
  (set-mark (point-at-bol))
  (if (bobp)
      (goto-char (point-max))
    (forward-sexp)
    (and (eolp) (forward-char)))
  (exchange-point-and-mark dont-activate))

(defun tcconf-comment-section (&optional arg)
  "Comment the current (innermost) section.
With C-u prefix arg, uncomment the section.
With numeric prefix arg ARG, comment ARG sections outward.
If ARG is negative, uncomment -ARG sections."
  (interactive "P")
  (save-excursion
    (tcconf-mark-section (cond ((numberp arg) (abs arg)) (1)) t)
    (comment-region (point) (mark t) (cond ((numberp arg)
					    (cond ((< arg 0)
						   '(4))
						  (nil)))
					   (arg)))))

(defun tcconf-kill-section (&optional arg)
  "Kill the current section and place it in the kill ring."
  (interactive "P")
  (tcconf-mark-section arg t)
  (kill-region (point) (mark t)))

(provide 'tcconf-mode)
