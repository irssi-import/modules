#include "module.h"
#include "formats.h"

FORMAT_REC idea_formats[] =
{
   { MODULE_NAME, "IDEA encryption module", 0 },

   /* ---- */

   { "idea_own_msg", "{ownmsgnick $2 {ownnick $0}}%B$1", 3, { 0, 0, 0 } },
   { "idea_own_msg_channel", "{ownmsgnick $3 {ownnick $0}{msgchannel $1}}%B$2", 4, { 0, 0, 0 } },
   { "idea_own_msg_private", "{ownprivmsg msg $0}%B$1", 2, { 0, 0 } },
   { "idea_own_msg_private_query", "{ownprivmsgnick {ownprivnick $2}}%B$1", 3, { 0, 0, 0 } },

   { "idea_pubmsg_me", "{pubmsgmenick $2 {menick $0}}%B$1", 3, { 0, 0, 0 } },
   { "idea_pubmsg_me_channel", "{pubmsgmenick $3 {menick $0}{msgchannel $1}}%B$2", 4, { 0, 0, 0, 0 } },
   { "idea_pubmsg_hilight", "{pubmsghinick $0 $3 $1}%B$2", 4, { 0, 0, 0, 0 } },
   { "idea_pubmsg_hilight_channel", "{pubmsghinick $0 $4 $1{msgchannel $2}}%B$3", 5, { 0, 0, 0, 0, 0 } },
   { "idea_pubmsg", "{pubmsgnick $2 {pubnick $0}}%B$1", 3, { 0, 0, 0 } },
   { "idea_pubmsg_channel", "{pubmsgnick $3 {pubnick $0}{msgchannel $1}}%B$2", 4, { 0, 0, 0, 0 } },
   { "idea_msg_private", "{privmsg $0 $1}%B$2", 3, { 0, 0, 0 } },
   { "idea_msg_private_query", "{privmsgnick $0}%B$2", 3, { 0, 0, 0 } },


   { NULL, NULL, 0 }
};

