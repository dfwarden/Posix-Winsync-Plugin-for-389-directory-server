/** BEGIN COPYRIGHT BLOCK
 * This Program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2 of the License.
 *
 * This Program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 *
 * You should have received a copy of the GNU General Public License along with
 * this Program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * END COPYRIGHT BLOCK **/
/* 
	$Id: posix-winsync-config.c 35 2011-05-27 11:53:40Z grzemba $
*/

#ifdef WINSYNC_TEST_POSIX
#include <slapi-plugin.h>
#include "winsync-plugin.h"
#else
#include <dirsrv/slapi-plugin.h>
#include <dirsrv/winsync-plugin.h>
#endif
#include "posix-wsp-ident.h"
#include <string.h>

#define POSIX_WINSYNC_CONFIG_FILTER "(objectclass=*)"
/*
 * static variables
 */
/* for now, there is only one configuration and it is global to the plugin  */
static POSIX_WinSync_Config theConfig;
static int inited = 0;

static int posix_winsync_apply_config (Slapi_PBlock *pb, Slapi_Entry* entryBefore,
                          Slapi_Entry* e, int *returncode, char *returntext,
                          void *arg);

POSIX_WinSync_Config *posix_winsync_get_config()
{
    return &theConfig;
}

PRBool posix_winsync_config_get_mapMemberUid()
{
    return theConfig.mapMemberUID;
}

PRBool posix_winsync_config_get_lowercase()
{
    return theConfig.lowercase;
}

PRBool posix_winsync_config_get_createMOFTask()
{
    return theConfig.createMemberOfTask;
}
void posix_winsync_config_set_MOFTaskCreated()
{
    theConfig.MOFTaskCreated = PR_TRUE;
}
void posix_winsync_config_reset_MOFTaskCreated()
{
    theConfig.MOFTaskCreated = PR_FALSE;
}
PRBool posix_winsync_config_get_MOFTaskCreated()
{
    return theConfig.MOFTaskCreated;
}

PRBool posix_winsync_config_get_msSFUSchema()
{
    return theConfig.mssfuSchema;
}

/*
 * Read configuration and create a configuration data structure.
 * This is called after the server has configured itself so we can check
 * schema and whatnot.
 * Returns an LDAP error code (LDAP_SUCCESS if all goes well).
 */
int posix_winsync_config(Slapi_Entry *config_e)
{
    int returncode = LDAP_SUCCESS;
    char returntext[SLAPI_DSE_RETURNTEXT_SIZE];

    slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
                    "--> _config %s -- begin\n",slapi_entry_get_dn_const(config_e));
    if ( inited ) {
        slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,
                         "Error: POSIX WinSync plug-in already configured.  "
                         "Please remove the plugin config entry [%s]\n",
                         slapi_entry_get_dn_const(config_e));
        return( LDAP_PARAM_ERROR );
    }

    /* initialize fields */
    if ((theConfig.lock = slapi_new_mutex()) == NULL) {
        return( LDAP_LOCAL_ERROR );
    }

    /* init defaults */
    theConfig.config_e = slapi_entry_alloc();
    slapi_entry_init(theConfig.config_e, slapi_ch_strdup(""), NULL);
    theConfig.mssfuSchema = PR_FALSE;
    theConfig.mapMemberUID = PR_TRUE;
    theConfig.lowercase = PR_FALSE;
    theConfig.createMemberOfTask = PR_FALSE;
    theConfig.MOFTaskCreated = PR_FALSE;
    posix_winsync_apply_config(NULL, NULL, config_e,&returncode, returntext, NULL);
    /* config DSE must be initialized before we get here */
    if (returncode == LDAP_SUCCESS) {
        const char *config_dn = slapi_entry_get_dn_const(config_e);
       slapi_config_register_callback(SLAPI_OPERATION_MODIFY, DSE_FLAG_POSTOP, config_dn, LDAP_SCOPE_BASE,
                                       POSIX_WINSYNC_CONFIG_FILTER, posix_winsync_apply_config,NULL);
    }

    inited = 1;

    if (returncode != LDAP_SUCCESS) {
        slapi_log_error(SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,
                        "Error %d: %s\n", returncode, returntext);
    }

    return returncode;
}

static int posix_winsync_apply_config (Slapi_PBlock *pb, Slapi_Entry* entryBefore,
                          Slapi_Entry* e, int *returncode, char *returntext,
                          void *arg)
{
    PRBool mssfuSchema = PR_FALSE;
    PRBool mapMemberUID = PR_TRUE;
    PRBool createMemberOfTask = PR_FALSE;
    PRBool lowercase = PR_FALSE;
    Slapi_Attr *testattr = NULL;

    *returncode = LDAP_UNWILLING_TO_PERFORM; /* be pessimistic */

    /* get msfuSchema value */
    if (!slapi_entry_attr_find(e, POSIX_WINSYNC_MSSFU_SCHEMA, &testattr) &&
        (NULL != testattr)) {
        mssfuSchema = slapi_entry_attr_get_bool(e, POSIX_WINSYNC_MSSFU_SCHEMA);
        slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
            "_apply_config: Config paramter %s: %d\n", POSIX_WINSYNC_MSSFU_SCHEMA, mssfuSchema);
    }

    /* get memberUid value */
    if (!slapi_entry_attr_find(e, POSIX_WINSYNC_MAP_MEMBERUID, &testattr) &&
        (NULL != testattr)) {
        mapMemberUID = slapi_entry_attr_get_bool(e, POSIX_WINSYNC_MAP_MEMBERUID);
        slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
            "_apply_config: Config paramter %s: %d\n", POSIX_WINSYNC_MAP_MEMBERUID, mapMemberUID);
    }
    /* get create task value */
    if (!slapi_entry_attr_find(e, POSIX_WINSYNC_CREATE_MEMBEROFTASK, &testattr) &&
        (NULL != testattr)) {
        createMemberOfTask = slapi_entry_attr_get_bool(e, POSIX_WINSYNC_CREATE_MEMBEROFTASK);
        slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
            "_apply_config: Config paramter %s: %d\n", POSIX_WINSYNC_CREATE_MEMBEROFTASK, createMemberOfTask);
    }
    /* get lower case UID in memberUID */
    if (!slapi_entry_attr_find(e, POSIX_WINSYNC_LOWER_CASE, &testattr) &&
        (NULL != testattr)) {
        lowercase = slapi_entry_attr_get_bool(e, POSIX_WINSYNC_LOWER_CASE);
        slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
            "_apply_config: Config paramter %s: %d\n", POSIX_WINSYNC_LOWER_CASE, lowercase);
    }
    /* if we got here, we have valid values for everything
       set the config entry */
    slapi_lock_mutex(theConfig.lock);
    slapi_entry_free(theConfig.config_e);
    theConfig.config_e = slapi_entry_alloc();
    slapi_entry_init(theConfig.config_e, slapi_ch_strdup(""), NULL);

    /* all of the attrs and vals have been set - set the other values */
    theConfig.mssfuSchema = mssfuSchema;
    theConfig.mapMemberUID = mapMemberUID;
    theConfig.createMemberOfTask = createMemberOfTask;
    theConfig.lowercase = lowercase;

    /* success */
    slapi_log_error(SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,
        "<-- _apply_config: config evaluated\n");
    *returncode = LDAP_SUCCESS;

done3:
    slapi_unlock_mutex(theConfig.lock);

    if (*returncode != LDAP_SUCCESS) {
        return SLAPI_DSE_CALLBACK_ERROR;
    } else {
        return SLAPI_DSE_CALLBACK_OK;
    }
}

