/* Copyright 2001-2005 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

static MP_INLINE
SV *mpxs_apr_ipsubnet_create(pTHX_ SV *classname, SV *p_sv,
                             const char *ipstr,
                             const char *mask_or_numbits)
{
    apr_pool_t *p = mp_xs_sv2_APR__Pool(p_sv);
    apr_ipsubnet_t *ipsub = NULL;
    SV *ipsub_sv;
    MP_RUN_CROAK(apr_ipsubnet_create(&ipsub, ipstr, mask_or_numbits, p),
                 "APR::IpSubnet::new");
    ipsub_sv = sv_setref_pv(NEWSV(0, 0), "APR::IpSubnet", (void*)ipsub);
    mpxs_add_pool_magic(ipsub_sv, p_sv);
    return ipsub_sv;
}
