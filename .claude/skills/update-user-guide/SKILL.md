---
name: update-user-guide
description: Updates the end-user guide in English and Brazilian Portuguese together, written for people who use scalers rather than firmware developers. Use when a user-visible behaviour, setting, status message, installation or recovery step changes, or when the user asks for user documentation.
---

The guide lives in `docs/user-guide/en/` and `docs/user-guide/pt-BR/`, with the same file names and
the same section structure. A change to one language is made to the other in the same commit.

1. Write for someone with a console, a scaler and a TV — not for a developer. No register names, no
   code. Use the setting labels exactly as the UI shows them (`schema/i18n/*.yaml`), including the
   Portuguese ones in the pt-BR guide.
2. Every status headline the board can show has an entry in `troubleshooting.md` saying what it
   means and what to try, in order of likelihood.
3. Steps are numbered and each says what the user should see when it worked.
4. The pt-BR text is natural Brazilian Portuguese (você, not tu; "placa", "entrada", "saída"),
   reviewed as its own text, not a literal translation.
5. Mention hardware switches by the label printed on the board, and say when the firmware cannot
   see their position.
6. Check both files render (headings, tables, links) and that relative links work from each
   language folder.
