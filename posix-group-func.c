/** Author: Carsten Grzemba grzemba@contac-dt.de>
 *
 * Copyright (C) 2011 contac Datentechnik GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 only
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	$Id: posix-group-func.c 28 2011-05-13 14:35:29Z grzemba $
*/
#include "slapi-plugin.h"
#include "slapi-private.h"

#include <string.h>
#include "posix-wsp-ident.h"

Slapi_Value **valueset_get_valuearray(const Slapi_ValueSet *vs); /* stolen from proto-slap.h */

Slapi_PBlock *searchDN( const char *baseDN, const char *filter, char *attrs[] ) {
    Slapi_PBlock *spb = NULL;
    int sres;

    /* Perform the search - the new pblock needs to be freed */
    spb = slapi_search_internal((char *)baseDN, LDAP_SCOPE_BASE,
                                                            (char *)filter, NULL, attrs, 0);
    if ( !spb ) {
            slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,"searchDN: error searching for uid: %s", baseDN);
            return NULL;
    }
    if ( slapi_pblock_get( spb, SLAPI_PLUGIN_INTOP_RESULT, &sres ) ) {
            slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,"searchDN: error getting search results for uid: %s", baseDN);
            return NULL;
    } else if (sres) {
            slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,"searchDN: no search results for uid: %s", baseDN);
            return NULL;
    }
    return spb;
}

/*
 * dnHasObjectClass - read an entry if it has a particular object class
 * Return:
 *   A pblock containing the entry, or NULL
 */
Slapi_PBlock *
dnHasObjectClass( const char *baseDN, const char *objectClass, Slapi_Entry **entry ) {
    char *filter = NULL;
    Slapi_PBlock *spb = NULL;
    char *attrs[2];
    Slapi_Entry **entries;

    
    /* Perform the search - the new pblock needs to be freed */
    attrs[0] = "objectclass";
    attrs[1] = NULL;
    filter = PR_smprintf("objectclass=%s", objectClass );
    if ( !(spb = searchDN( baseDN, filter, attrs) ) ) {
            PR_smprintf_free(filter);
            return NULL;
    }
    PR_smprintf_free(filter);
    if ( slapi_pblock_get(spb, SLAPI_PLUGIN_INTOP_SEARCH_ENTRIES,
                                              &entries) ) {
            slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,"dnHasObjectClass: no search results for uid: %s", baseDN);
            return NULL;
    }
    /*
     * Can only be one entry returned on a base search; just check
     * the first one
     */
    if ( !*entries ) {
            /* Clean up */
            slapi_free_search_results_internal(spb);
            slapi_pblock_destroy(spb);
            return NULL;
    }
    *entry = *entries;
    return spb;
}

/* search the user with DN udn and returns uid*/
char * searchUid(const char *udn) {
    int         rc = 0;

    Slapi_PBlock *int_search_pb = slapi_pblock_new ();
    Slapi_Entry **entries = NULL;
    char *attrs[]={"uid",NULL};
    char *uid=NULL;
    
    slapi_log_error(SLAPI_LOG_PLUGIN,POSIX_WINSYNC_PLUGIN_NAME,"search Uid: search %s\n", udn);
    
    slapi_search_internal_set_pb ( int_search_pb, 
                                   udn, 
                                   LDAP_SCOPE_BASE, 
                                   "(|(objectclass=posixAccount)(objectclass=ldapsubentry))",
                                   attrs ,
                                   0 /* attrsonly */, NULL /* controls */,
                                   NULL /* uniqueid */,
                                   posix_winsync_get_plugin_identity(), 0 /* actions */ );
    if (slapi_search_internal_pb ( int_search_pb )) {
          /* get result and log an error */
          int res = 0;
          slapi_pblock_get(int_search_pb, SLAPI_PLUGIN_INTOP_RESULT, &res);
          slapi_log_error( SLAPI_LOG_FATAL, POSIX_WINSYNC_PLUGIN_NAME,"searchUid: error searching for uid: %d", res);
    }else{
        slapi_log_error(SLAPI_LOG_PLUGIN,POSIX_WINSYNC_PLUGIN_NAME,"searchUid: searched %s\n", udn);
        slapi_pblock_get( int_search_pb, SLAPI_PLUGIN_INTOP_SEARCH_ENTRIES, &entries );
        if ( NULL != entries && NULL != entries[ 0 ]) {
            Slapi_Attr *attr=NULL;
            Slapi_Value *v=NULL;
            
            if ( slapi_entry_attr_find(entries[0],"uid",&attr) == 0){
                slapi_attr_first_value(attr,&v);
                uid = slapi_ch_strdup(slapi_value_get_string(v));
                slapi_log_error(SLAPI_LOG_PLUGIN,POSIX_WINSYNC_PLUGIN_NAME,"searchUid: return uid %s\n", uid);
                /* slapi_value_free(&v); */
            }else{
                slapi_log_error(SLAPI_LOG_PLUGIN,POSIX_WINSYNC_PLUGIN_NAME,"searchUid: uid in %s not found\n", udn);
            }                
            slapi_free_search_results_internal(int_search_pb);
            slapi_pblock_destroy(int_search_pb);
            if (posix_winsync_config_get_lowercase()) {
                return slapi_dn_ignore_case(uid);
            }
            return uid;
        }
    }
    slapi_free_search_results_internal(int_search_pb);
    slapi_pblock_destroy(int_search_pb);
    slapi_log_error(SLAPI_LOG_PLUGIN,POSIX_WINSYNC_PLUGIN_NAME,"searchUid: posix user %s not found\n", udn);
    return NULL;
}

int dn_in_set(const char* uid, char **uids) {
    int i;
    Slapi_DN *sdn_uid=slapi_sdn_new_dn_byval(uid);
    Slapi_DN *sdn_ul=slapi_sdn_new();
    
    if (uids == NULL ||uid == NULL) return false;
    for(i=0;uids[i];i++){
        slapi_sdn_set_dn_byref(sdn_ul,uids[i]);
        if(slapi_sdn_compare(sdn_uid,sdn_ul)==0){
            slapi_sdn_free(&sdn_ul);
            slapi_sdn_free(&sdn_uid);
            return true;
        }
        slapi_sdn_done(sdn_ul);
    }
    slapi_sdn_free(&sdn_ul);
    slapi_sdn_free(&sdn_uid);
    return false;
}

int uid_in_set(const char* uid, char **uids) {
    int i;

    if (uids == NULL ||uid == NULL) return false;
    for(i=0;uids[i];i++){
        Slapi_RDN *i_rdn=NULL;
        char *i_uid=NULL;
        char *t=NULL;
        
        i_rdn=slapi_rdn_new_dn(uids[i]);
        if (slapi_rdn_get_first(i_rdn,&t,&i_uid)==1){
            if(strncasecmp(uid,i_uid,256)==0){
                slapi_rdn_free(&i_rdn);
                return true;
            }
        }
        slapi_rdn_free(&i_rdn);
    }
    return false;
}

int modGroupMembership(Slapi_Entry *entry, Slapi_Mods *smods, int *do_modify){
    int rc=0;
    Slapi_Attr  * obj_attr = NULL;           /* Entry attributes        */

    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: ==>\n");
    
    rc = slapi_entry_attr_find(entry, "objectclass", &obj_attr);
    if (rc == 0) {                 /* Found objectclasses, so...  */
        int i;
        Slapi_Value * value = NULL;               /* Attribute values        */
        
        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership scan objectclasses\n");
        for (
            i = slapi_attr_first_value(obj_attr, &value);
            i != -1;
            i = slapi_attr_next_value(obj_attr, i, &value)
        ) {
            Slapi_Attr  * um_attr = NULL;           /* Entry attributes uniquemember        */
            Slapi_Attr  * muid_attr = NULL;           /* Entry attributes memebrof       */
            Slapi_Value * uid_value =NULL;               /* uniquemember Attribute values        */
            const char * oc = NULL;
            
            oc = slapi_value_get_string(value);
            if (strncasecmp(oc,"posixGroup",11)==0){ /* entry has objectclass posixGroup */
                Slapi_ValueSet *newvs = NULL;
                Slapi_Mod *smod=slapi_mod_new();
                Slapi_Mod *nextMod=slapi_mod_new();
                int um_mod = 0;
                char **uids=NULL;
                
                slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: posixGroup -> look for uniquemember\n");
                if (slapi_is_loglevel_set(SLAPI_LOG_PLUGIN)) 
                    slapi_mods_dump(smods,"memberUid - mods dump - initial");
                for (smod=slapi_mods_get_first_smod(smods, nextMod);smod;smod=slapi_mods_get_next_smod(smods,nextMod)) {
                    if (slapi_attr_types_equivalent(slapi_mod_get_type(smod),"uniqueMember")){
                        struct berval   *bv;
                        
                        um_mod=slapi_mod_get_operation(smod);
                        for (bv = slapi_mod_get_first_value(smod);bv; bv = slapi_mod_get_next_value(smod)) {
                            Slapi_Value *sv = slapi_value_new();
                            
                            slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: add to uidmodlist %s\n",bv->bv_val);
                            slapi_value_init_berval(sv, bv); /* copies bv_val */
                            slapi_ch_array_add(&uids,slapi_ch_strdup(slapi_value_get_string(sv)));
                            slapi_value_free(&sv);
                        }
                    }
                }
                slapi_mod_done(nextMod);
                if (!um_mod){
                    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: no uniquemember mod, nothing to do<==\n");
                    return 0;
                }
                
                int i;
                
                slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: entry is posixGroup\n");
        
                Slapi_Attr  * muid_attr = NULL;           /* Entry attributes        */
                Slapi_Value * uid_value =NULL;               /* Attribute values        */
                    
                char **adduids=NULL;
                char **moduids=NULL;
                char **deluids=NULL;
                int doModify=false;
                int j=0;
                
                if (SLAPI_IS_MOD_DELETE(um_mod)){
                    Slapi_Attr  * mu_attr = NULL;           /* Entry attributes        */
                    rc = slapi_entry_attr_find(entry, "memberUid", &mu_attr);
                    if (rc != 0 || mu_attr == NULL) {
                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership end: attribute memberUid not found\n");
                        return 0;
                    }
                    /* found attribute uniquemember */
                    if (uids==NULL){ /* deletion of the last value, deletes the Attribut from entry complete, this operation has no value, so we must look by self */
                        Slapi_Attr  * um_attr = NULL;           /* Entry attributes        */
                        Slapi_Value * uid_dn_value = NULL;               /* Attribute values        */
                        int rc = slapi_entry_attr_find(entry, "uniquemember", &um_attr);
                        if (rc != 0 || um_attr == NULL) {
                            slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership end: attribute uniquemember not found\n");
                            return 0;
                        }
                        /* found attribute uniquemember */
                        for (                          /* ...loop for value...    */
                            j = slapi_attr_first_value(um_attr, &uid_dn_value);
                            j != -1;
                            j = slapi_attr_next_value(um_attr, j, &uid_dn_value)
                        ) {
                            slapi_ch_array_add(&uids,slapi_ch_strdup(slapi_value_get_string(uid_dn_value)));
                        }
                    }
                    for (                          /* ...loop for value...    */
                        j = slapi_attr_first_value(mu_attr, &uid_value);
                        j != -1;
                        j = slapi_attr_next_value(mu_attr, j, &uid_value)
                    ) {
                                                                         /* remove from uniquemember: remove from memberUid also */
                        const char *uid=NULL;
                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: test dellist \n");
                        uid=slapi_value_get_string(uid_value);
                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: test dellist %s\n",uid);
                        if (uid_in_set(uid,uids)){
                            slapi_ch_array_add(&deluids,slapi_ch_strdup(uid));    
                            slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: add to dellist %s\n",uid);
                            doModify=true;
                        }
                    }
                }else{
                    const char *uid_dn=NULL;
        
                    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: posixGroup -> look for uniquemember\n");
                    /* found attribute uniquemember */
                    for (j = 0;uids[j];j++){
                        static char *uid=NULL;
                        
                        uid_dn=uids[j];
                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: perform user %s\n", uid_dn);
        
                        uid=searchUid(uid_dn);
        
                        if (uid == NULL) {
                            slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: uid not found for %s, cannot do anything\n", uid_dn);  /* member on longer on server, do nothing */
                        } else {
                            rc |= slapi_entry_attr_find(entry, "memberUid", &muid_attr);
                            if ( rc != 0 || muid_attr == NULL ) {     /* Found no memberUid list, so create  */
                                slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: no attribute memberUid, add with %s \n", uid_dn);
                                slapi_ch_array_add(&adduids, uid);    
                                doModify=true;
                            }else {                                                                 /* Found a memberUid list, so modify */
                                if (SLAPI_IS_MOD_ADD(um_mod)){
                                    Slapi_ValueSet *vs=NULL;
                                    Slapi_Value *v=slapi_value_new();
                                    
                                    slapi_value_init_string_passin(v,uid);
                                    slapi_attr_get_valueset(muid_attr,&vs);
                                    if (slapi_valueset_find(muid_attr,vs,v)!=NULL ){  /* already exist, all ok */
                                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: uid found in memberuid list %s nothing to do\n", uid);
                                    }else{
                                        slapi_ch_array_add(&moduids, uid);    
                                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: add to modlist %s\n",uid);
                                        doModify=true;
                                    }
                                }
        /*                                slapi_value_free(&v); */
                            }
                        }
                    }
                }
                if (doModify){
                    if (adduids){
                        int i;
                        for (i=0; adduids[i]; i++) {
                            slapi_mods_add_string(smods,LDAP_MOD_ADD,"memberUid",adduids[i]);
                        }
                        slapi_ch_array_free(adduids);adduids=NULL;       
                    }else{
                        int i;
                        for (i=0; moduids && moduids[i]; i++) {
                            slapi_mods_add_string(smods,LDAP_MOD_ADD,"memberUid",moduids[i]);
                        }
                        slapi_ch_array_free(moduids);moduids=NULL;
                        for (i=0; deluids && deluids[i]; i++) {
                            slapi_mods_add_string(smods,LDAP_MOD_DELETE,"memberUid",deluids[i]);
                        }
                        slapi_ch_array_free(deluids);deluids=NULL;
                    }
                    if (slapi_is_loglevel_set(SLAPI_LOG_PLUGIN)) 
                        slapi_mods_dump(smods,"memberUid - mods dump");
                    *do_modify = 1;
                    posix_winsync_config_set_MOFTaskCreated();
              
                    break;     
                }
            }
        }
    }
    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"modGroupMembership: <==\n");
    return 0;
}

int addGroupMembership(Slapi_Entry *entry, Slapi_Entry *ad_entry){
    int rc=0;
    Slapi_Attr  * obj_attr = NULL;           /* Entry attributes        */

    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: ==>\n");
    
    rc = slapi_entry_attr_find(entry, "objectclass", &obj_attr);
    if (rc == 0) {                 /* Found objectclasses, so...  */
        int i;
        Slapi_Value * value = NULL;               /* Attribute values        */
        
        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership scan objectclasses\n");
        for (
            i = slapi_attr_first_value(obj_attr, &value);
            i != -1;
            i = slapi_attr_next_value(obj_attr, i, &value)
        ) {
            Slapi_Attr  * um_attr = NULL;           /* Entry attributes uniquemember        */
            Slapi_Attr  * muid_attr = NULL;           /* Entry attributes memebrof       */
            Slapi_Value * uid_value =NULL;               /* uniquemember Attribute values        */
            const char * oc = NULL;
            
            oc = slapi_value_get_string(value);
            if (strncasecmp(oc,"posixGroup",11)==0){ /* entry has objectclass posixGroup */
                Slapi_ValueSet *newvs = NULL;
                
                slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: posixGroup -> look for uniquemember\n");
                rc = slapi_entry_attr_find(entry, "uniquemember", &um_attr);
                if (rc != 0 || um_attr == NULL) {
                    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership end: attribute uniquemember not found\n");
                    return 0;
                }
                /* found attribute uniquemember */
                rc = slapi_entry_attr_find(entry, "memberUid", &muid_attr);
                if (rc != 0 || muid_attr == NULL ) {                                     /* Found no memberUid list, so create  */
                    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: no attribute memberUid\n");
                }
                newvs=slapi_valueset_new();

                
                for (                          /* ...loop for value...    */
                    i = slapi_attr_first_value(um_attr, &uid_value);
                    i != -1;
                    i = slapi_attr_next_value(um_attr, i, &uid_value)
                ) {
                    const char *uid_dn=NULL;
                    static char *uid=NULL;
                    const Slapi_Value *v=NULL;
                    
                    uid_dn=slapi_value_get_string(uid_value);
                    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: perform member %s\n", uid_dn);
                    uid=searchUid(uid_dn);
                    if (uid == NULL) {
                        slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: uid not found for %s, cannot do anything\n", uid_dn);  /* member on longer on server, do nothing */
                    } else {
                        v=slapi_value_new_string(uid);
                        if (slapi_attr_value_find(muid_attr , slapi_value_get_berval(v)) == 0) {
                            slapi_ch_free_string(&uid);
                            continue;
                        }
                        slapi_valueset_add_value(newvs,v);
                        slapi_ch_free_string(&uid);
                    }
                }
                slapi_entry_add_valueset(entry,"memberUid", newvs);
                slapi_valueset_free(newvs);
                posix_winsync_config_get_MOFTaskCreated();
          
                break;        
            }
        }
    }
    slapi_log_error( SLAPI_LOG_PLUGIN, POSIX_WINSYNC_PLUGIN_NAME,"addGroupMembership: <==\n");
    return 0;
}

