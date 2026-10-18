#include "search.h"
#include "i18n.h"
#include "comm.h"
#include "memo_list.h"
#include "toast.h"
#include "ui.h"

static DictationSession *s_dictation;

static void dictation_callback(DictationSession *session,
                               DictationSessionStatus status,
                               char *transcription, void *context) {
  if (status != DictationSessionStatusSuccess) {
    // no error message if the user cancelled the dictation
    if (status != DictationSessionStatusFailureTranscriptionRejected &&
        status != DictationSessionStatusFailureSystemAborted) {
      toast_show(tr("Dictation Failed"), false);
    }
    return;
  }
  if (ui_is_blank(transcription)) {
    return;
  }

  // the phone matches words, so trailing punctuation would get in the way
  char query[COMM_TITLE_SIZE];
  ui_copy_truncated(query, transcription, sizeof(query));
  size_t len = strlen(query);
  while (len > 0 && strchr(".,!? ", query[len - 1])) {
    query[--len] = '\0';
  }
  memo_list_push_search(query);
}

void search_start(void) {
  if (!s_dictation) {
    // a short query needs no confirmation screen
    s_dictation = dictation_session_create(0, dictation_callback, NULL);
    if (s_dictation) {
      dictation_session_enable_confirmation(s_dictation, false);
    }
  }
  if (s_dictation) {
    dictation_session_start(s_dictation);
  }
}
