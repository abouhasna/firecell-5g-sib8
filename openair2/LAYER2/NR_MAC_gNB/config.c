/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file config.c
 * \brief gNB configuration performed by RRC or as a consequence of RRC procedures
 * \author  Navid Nikaein and Raymond Knopp, WEI-TAI CHEN
 * \date 2010 - 2014, 2018
 * \version 0.1
 * \company Eurecom, NTUST
 * \email: navid.nikaein@eurecom.fr, kroempa@gmail.com
 * @ingroup _mac

 */

#include "common/platform_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include "NR_BCCH-BCH-Message.h"
#include "NR_ServingCellConfigCommon.h"
#include "uper_encoder.h"

#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "SCHED_NR/phy_frame_config_nr.h"
#include "openair1/PHY/defs_gNB.h"

#include "NR_MIB.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "../../../../nfapi/oai_integration/vendor_ext.h"
/* Softmodem params */
#include "executables/softmodem-common.h"
#include <complex.h>

extern RAN_CONTEXT_t RC;
//extern int l2_init_gNB(void);
extern uint8_t nfapi_mode;

c16_t convert_precoder_weight(double complex c_in)
{
  double cr = creal(c_in) * 32768 + 0.5;
  if (cr < 0)
    cr -= 1;
  double ci = cimag(c_in) * 32768 + 0.5;
  if (ci < 0)
    ci -= 1;
  return (c16_t) {.r = (short)cr, .i = (short)ci};
}

void get_K1_K2(int N1, int N2, int *K1, int *K2, int layers)
{
  // num of allowed k1 and k2 according to 5.2.2.2.1-3 and -4 in 38.214
  switch (layers) {
    case 1:
      *K1 = 1;
      *K2 = 1;
      break;
    case 2:
      *K2 = N2 == 1 ? 1 : 2;
      if(N2 == N1 || N1 == 2)
        *K1 = 2;
      else if (N2 == 1)
        *K1 = 4;
      else
        *K1 = 3;
      break;
    case 3:
    case 4:
      *K2 = N2 == 1 ? 1 : 2;
      if (N1 == 6)
        *K1 = 5;
      else
        *K1 = N1;
      break;
    default:
      AssertFatal(false, "Number of layers %d not supported\n", layers);
  }
}

int get_NTN_Koffset(const NR_ServingCellConfigCommon_t *scc)
{
  if (scc->ext2 && scc->ext2->ntn_Config_r17 && scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17)
    return *scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17 << *scc->ssbSubcarrierSpacing;
  return 0;
}

int precoding_weigths_generation(nfapi_nr_pm_list_t *mat,
                                 int pmiq,
                                 int L,
                                 int N1,
                                 int N2,
                                 int O1,
                                 int O2,
                                 int num_antenna_ports,
                                 double complex theta_n[4],
                                 double complex v_lm[N1 * O1 + 4 * O1][N2 * O2 + O2][N2 * N1])
{
  // Table 5.2.2.2.1-X:
  // Codebook for L-layer CSI reporting using antenna ports 3000 to 2999+PCSI-RS
  // pmi=1,...,pmi_size are computed as follows
  int K1 = 0, K2 = 0;
  get_K1_K2(N1, N2, &K1, &K2, L);
  int I2 = L == 1 ? 4 : 2;
  for (int k2 = 0; k2 < K2; k2++) {
    for (int k1 = 0; k1 < K1; k1++) {
      for (int mm = 0; mm < N2 * O2; mm++) { // i_1_2
        for (int ll = 0; ll < N1 * O1; ll++) { // i_1_1
          for (int nn = 0; nn < I2; nn++) { // i_2
            mat->pmi_pdu[pmiq].pm_idx = pmiq + 1; // index 0 is the identity matrix
            mat->pmi_pdu[pmiq].numLayers = L;
            mat->pmi_pdu[pmiq].num_ant_ports = num_antenna_ports;
            LOG_D(PHY, "layer %d Codebook pmiq = %d\n", L, pmiq);
            for (int j_col = 0; j_col < L; j_col++) {
              int llc = ll + (k1 * O1 * (j_col & 1));
              int mmc = mm + (k2 * O2 * (j_col & 1));
              double complex phase_sign = j_col <= ((L - 1) / 2) ? 1 : -1;
              double complex res_code;
              for (int i_rows = 0; i_rows < N1 * N2; i_rows++) {
                nfapi_nr_pm_weights_t *weights = &mat->pmi_pdu[pmiq].weights[j_col][i_rows];
                res_code = sqrt(1 / (double)(L * num_antenna_ports)) * v_lm[llc][mmc][i_rows];
                c16_t precoder_weight = convert_precoder_weight(res_code);
                weights->precoder_weight_Re = precoder_weight.r;
                weights->precoder_weight_Im = precoder_weight.i;
                LOG_D(PHY,
                      "%d Layer Precoding Matrix[pmi %d][antPort %d][layerIdx %d]= %f+j %f -> Fixed Point %d+j %d \n",
                      L,
                      pmiq,
                      i_rows,
                      j_col,
                      creal(res_code),
                      cimag(res_code),
                      weights->precoder_weight_Re,
                      weights->precoder_weight_Im);
              }
              for (int i_rows = N1 * N2; i_rows < 2 * N1 * N2; i_rows++) {
                nfapi_nr_pm_weights_t *weights = &mat->pmi_pdu[pmiq].weights[j_col][i_rows];
                res_code = sqrt(1 / (double)(L * num_antenna_ports)) * (phase_sign)*theta_n[nn] * v_lm[llc][mmc][i_rows - N1 * N2];
                c16_t precoder_weight = convert_precoder_weight(res_code);
                weights->precoder_weight_Re = precoder_weight.r;
                weights->precoder_weight_Im = precoder_weight.i;
                LOG_D(PHY,
                      "%d Layer Precoding Matrix[pmi %d][antPort %d][layerIdx %d]= %f+j %f -> Fixed Point %d+j %d \n",
                      L,
                      pmiq,
                      i_rows,
                      j_col,
                      creal(res_code),
                      cimag(res_code),
                      weights->precoder_weight_Re,
                      weights->precoder_weight_Im);
              }
            }
            pmiq++;
          }
        }
      }
    }
  }
  return pmiq;
}

nfapi_nr_pm_list_t init_DL_MIMO_codebook(gNB_MAC_INST *gNB, nr_pdsch_AntennaPorts_t antenna_ports)
{
  int num_antenna_ports = antenna_ports.N1 * antenna_ports.N2 * antenna_ports.XP;
  if (num_antenna_ports < 2)
    return (nfapi_nr_pm_list_t) {0};

  //NR Codebook Generation for codebook type1 SinglePanel
  int N1 = antenna_ports.N1;
  int N2 = antenna_ports.N2;
  //Uniform Planner Array: UPA
  //    X X X X ... X
  //    X X X X ... X
  // N2 . . . . ... .
  //    X X X X ... X
  //   |<-----N1---->|

  //Get the uniform planar array parameters
  // To be confirmed
  int O2 = N2 > 1 ? 4 : 1; //Vertical beam oversampling (1 or 4)
  int O1 = num_antenna_ports > 2 ? 4 : 1; //Horizontal beam oversampling (1 or 4)

  int max_mimo_layers = (num_antenna_ports < NR_MAX_NB_LAYERS) ? num_antenna_ports : NR_MAX_NB_LAYERS;
  AssertFatal(max_mimo_layers <= 4, "Max number of layers supported is 4\n");
  AssertFatal(num_antenna_ports < 16, "Max number of antenna ports supported is currently 16\n");

  int K1 = 0, K2 = 0;
  nfapi_nr_pm_list_t mat = {.num_pm_idx = 0};
  for (int i = 0; i < max_mimo_layers; i++) {
    get_K1_K2(N1, N2, &K1, &K2, i + 1);
    int i2_size = i == 0 ? 4 : 2;
    gNB->precoding_matrix_size[i] = i2_size * N1 * O1 * N2 * O2 * K1 * K2;
    mat.num_pm_idx += gNB->precoding_matrix_size[i];
  }

  mat.pmi_pdu = malloc16(mat.num_pm_idx * sizeof(*mat.pmi_pdu));
  AssertFatal(mat.pmi_pdu != NULL, "out of memory\n");

  // Generation of codebook Type1 with codebookMode 1 (num_antenna_ports < 16)

  // Generate DFT vertical beams
  // ll: index of a vertical beams vector (represented by i1_1 in TS 38.214)
  const int max_l = N1 * O1 + 4 * O1;  // max k1 is 4*O1
  double complex v[max_l][N1];
  for (int ll = 0; ll < max_l; ll++) { // i1_1
    for (int nn = 0; nn < N1; nn++) {
      v[ll][nn] = cexp(I * (2 * M_PI * nn * ll) / (N1 * O1));
      LOG_D(PHY, "v[%d][%d] = %f +j %f\n", ll, nn, creal(v[ll][nn]), cimag(v[ll][nn]));
    }
  }
  // Generate DFT Horizontal beams
  // mm: index of a Horizontal beams vector (represented by i1_2 in TS 38.214)
  const int max_m = N2 * O2 + O2; // max k2 is O2
  double complex u[max_m][N2];
  for (int mm = 0; mm < max_m; mm++) { // i1_2
    for (int nn = 0; nn < N2; nn++) {
      u[mm][nn] = cexp(I * (2 * M_PI * nn * mm) / (N2 * O2));
      LOG_D(PHY, "u[%d][%d] = %f +j %f\n", mm, nn, creal(u[mm][nn]), cimag(u[mm][nn]));
    }
  }
  // Generate co-phasing angles
  // i_2: index of a co-phasing vector
  // i1_1, i1_2, and i_2 are reported from UEs
  double complex theta_n[4];
  for (int nn = 0; nn < 4; nn++) {
    theta_n[nn] = cexp(I * M_PI * nn / 2);
    LOG_D(PHY, "theta_n[%d] = %f +j %f\n", nn, creal(theta_n[nn]), cimag(theta_n[nn]));
  }
  // Kronecker product v_lm
  double complex v_lm[max_l][max_m][N2 * N1];
  // v_ll_mm_codebook denotes the elements of a precoding matrix W_i1,1_i_1,2
  for (int ll = 0; ll < max_l; ll++) { // i_1_1
    for (int mm = 0; mm < max_m; mm++) { // i_1_2
      for (int nn1 = 0; nn1 < N1; nn1++) {
        for (int nn2 = 0; nn2 < N2; nn2++) {
          v_lm[ll][mm][nn1 * N2 + nn2] = v[ll][nn1] * u[mm][nn2];
          LOG_D(PHY,
                "v_lm[%d][%d][%d] = %f +j %f\n",
                ll,
                mm,
                nn1 * N2 + nn2,
                creal(v_lm[ll][mm][nn1 * N2 + nn2]),
                cimag(v_lm[ll][mm][nn1 * N2 + nn2]));
        }
      }
    }
  }

  int pmiq = 0;
  for (int layers = 1; layers <= max_mimo_layers; layers++)
    pmiq = precoding_weigths_generation(&mat, pmiq, layers, N1, N2, O1, O2, num_antenna_ports, theta_n, v_lm);

  return mat;
}

static int8_t set_tdd_bmap_period(NR_TDD_UL_DL_Pattern_t pattern, tdd_config_t *tdd_config, int8_t curr_total_slot)
{
  int8_t n_dl_slot, n_ul_slot, n_dl_symbols, n_ul_symbols, total_slot = 0;

  n_dl_slot = pattern.nrofDownlinkSlots;
  n_ul_slot = pattern.nrofUplinkSlots;
  n_dl_symbols = pattern.nrofDownlinkSymbols;
  n_ul_symbols = pattern.nrofUplinkSymbols;

  LOG_I(NR_MAC,
        "Setting TDD configuration period sum of both patterns dl_slot %d  ul_slot%d dl_sym %d ul_sym %d\n",
        n_dl_slot,
        n_ul_slot,
        n_dl_symbols,
        n_ul_symbols);

  total_slot = !(n_ul_symbols + n_dl_symbols) ? n_dl_slot + n_ul_slot : n_dl_slot + n_ul_slot + 1;
  tdd_config->num_dl_slots += n_dl_slot;
  tdd_config->num_ul_slots += (n_ul_slot + (n_ul_symbols > 0));

  for (int i = 0; i < total_slot; i++) {
    if (i < n_dl_slot)
      tdd_config->tdd_slot_bitmap[i + curr_total_slot].slot_type = TDD_NR_DOWNLINK_SLOT;
    else if ((i == n_dl_slot) && (n_ul_symbols + n_dl_symbols)) {
      tdd_config->tdd_slot_bitmap[i + curr_total_slot].slot_type = TDD_NR_MIXED_SLOT;
      tdd_config->tdd_slot_bitmap[i + curr_total_slot].num_dl_symbols = n_dl_symbols;
      tdd_config->tdd_slot_bitmap[i + curr_total_slot].num_ul_symbols = n_ul_symbols;
    } else if (n_ul_slot)
      tdd_config->tdd_slot_bitmap[i + curr_total_slot].slot_type = TDD_NR_UPLINK_SLOT;
  }

  LOG_I(NR_MAC, "Setting TDD configuration total slot %d and curr_slot %d\n", total_slot, curr_total_slot);

  return total_slot;
}
static int get_tdd_period(NR_TDD_UL_DL_ConfigCommon_t *tdd, nfapi_nr_config_request_scf_t *cfg, tdd_config_t *tdd_config)
{
  int num_of_patterns = 1;
  float tdd_ms_period_pattern[] = {0.5, 0.625, 1.0, 1.25, 2.0, 2.5, 5.0, 10.0, 20.0};
  float tdd_ms_period_ext[] = {3.0, 4.0};
  float pattern1_ms = 0.0, pattern2_ms = 0.0;
  int8_t total_slot_pattern1 = 0;
  tdd_config->num_dl_slots = 0;
  tdd_config->num_ul_slots = 0;
  NR_TDD_UL_DL_Pattern_t pattern = tdd->pattern1;

  if (pattern.ext1 == NULL) {
    LOG_D(NR_MAC, "Setting TDD configuration period to dl_UL_TransmissionPeriodicity %ld\n", pattern.dl_UL_TransmissionPeriodicity);
    pattern1_ms = tdd_ms_period_pattern[pattern.dl_UL_TransmissionPeriodicity];
  } else {
    AssertFatal(pattern.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL,
                "In %s: scc->tdd_UL_DL_ConfigurationCommon->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 is null\n",
                __FUNCTION__);
    LOG_D(NR_MAC,
          "Setting TDD configuration period to dl_UL_TransmissionPeriodicity_v1530 %ld\n",
          *pattern.ext1->dl_UL_TransmissionPeriodicity_v1530);
    pattern1_ms = tdd_ms_period_ext[*pattern.ext1->dl_UL_TransmissionPeriodicity_v1530];
  }

  total_slot_pattern1 = set_tdd_bmap_period(pattern, tdd_config, 0);

  if (tdd->pattern2) {
    num_of_patterns++;
    pattern = *tdd->pattern2;
    if (pattern.ext1 == NULL) {
      LOG_D(NR_MAC,
            "Setting TDD Pattern2 configuration period to dl_UL_TransmissionPeriodicity %ld\n",
            pattern.dl_UL_TransmissionPeriodicity);
      pattern2_ms = tdd_ms_period_pattern[pattern.dl_UL_TransmissionPeriodicity];

    } else {
      AssertFatal(pattern.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL,
                  "In %s: scc->tdd_UL_DL_ConfigurationCommon->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 is null\n",
                  __FUNCTION__);
      LOG_D(NR_MAC,
            "Setting TDD Pattern2 configuration period to dl_UL_TransmissionPeriodicity_v1530 %p\n",
            pattern.ext1->dl_UL_TransmissionPeriodicity_v1530);
      pattern2_ms = tdd_ms_period_ext[*pattern.ext1->dl_UL_TransmissionPeriodicity_v1530];
    }
    set_tdd_bmap_period((pattern), tdd_config, total_slot_pattern1);
  }
  bool found_match = false;
  for (int i = 0; i <= 8; i++) {
    if ((pattern1_ms + pattern2_ms) == tdd_ms_period_pattern[i]) {
      LOG_I(NR_MAC,
            "Setting TDD configuration period value in cfg->tdd_table.tdd_period based on the sum of dl_UL_TransmissionPeriodicity "
            "from Pattern1 (%f ms) and Pattern2 (%f ms): Total = %f ms\n",
            pattern1_ms,
            pattern2_ms,
            pattern1_ms + pattern2_ms);
      cfg->tdd_table.tdd_period.value = i;
      found_match = true;
      break;
    }
  }
  // Assert if no match was found
  AssertFatal(found_match, "The sum of pattern1_ms and pattern2_ms does not match any value in tdd_ms_period_pattern");

  LOG_I(NR_MAC, "Setting TDD configuration period sum of both patterns  %d\n", cfg->tdd_table.tdd_period.value);
  return num_of_patterns;
}

static int set_tdd_config_nr_new(nfapi_nr_config_request_scf_t *cfg, int mu, tdd_config_t *tdd_config)
{
  int slot_number = 0;
  tdd_config->tdd_numb_slots_frame = TDD_CONFIG_NB_FRAMES * (1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
  int nb_slots_to_set = tdd_config->tdd_numb_slots_frame;
  tdd_config->tdd_numb_period_frame = get_nb_periods_per_frame(cfg->tdd_table.tdd_period.value);
  int slot_index = 0;
  tdd_config->tdd_numb_slots_period = ((1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME) / tdd_config->tdd_numb_period_frame;

  int nb_slots_per_period = tdd_config->tdd_numb_slots_period;

  cfg->tdd_table.max_tdd_periodicity_list = calloc(nb_slots_to_set, sizeof(nfapi_nr_max_tdd_periodicity_t));

  for (int memory_alloc = 0; memory_alloc < nb_slots_to_set; memory_alloc++) {
    nfapi_nr_max_tdd_periodicity_t *max_tdd_periodicity_list = &cfg->tdd_table.max_tdd_periodicity_list[memory_alloc];
    max_tdd_periodicity_list->max_num_of_symbol_per_slot_list =
        calloc(NR_NUMBER_OF_SYMBOLS_PER_SLOT, sizeof(nfapi_nr_max_num_of_symbol_per_slot_t));
  }

  while (slot_number != nb_slots_to_set) {
    slot_index = slot_number % nb_slots_per_period;
    if (tdd_config->tdd_slot_bitmap[slot_index].slot_type == TDD_NR_DOWNLINK_SLOT) {
      for (int number_of_symbol = 0; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config.value =
            0;
        LOG_I(NR_MAC,
              "Setting TDD configuration slot_number %d is DL_Slot %d slot_index %d DL_symbol \n",
              slot_number,
              slot_index,
              number_of_symbol);
      }
      LOG_I(NR_MAC, "Setting TDD configuration slot_number %d is DL_Slot %d slot_index \n", slot_number, slot_index);
      slot_number++;
    }

    if (tdd_config->tdd_slot_bitmap[slot_index].slot_type == TDD_NR_MIXED_SLOT) {
      for (int number_of_symbol = 0; number_of_symbol < tdd_config->tdd_slot_bitmap[slot_index].num_dl_symbols;
           number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config.value =
            0;
        LOG_I(NR_MAC,
              "Setting TDD configuration slot_number %d is Mixed Slot %d slot_index %d dl_symbol \n",
              slot_number,
              slot_index,
              number_of_symbol);
      }

      for (int number_of_symbol = tdd_config->tdd_slot_bitmap[slot_index].num_dl_symbols;
           number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT - tdd_config->tdd_slot_bitmap[slot_index].num_ul_symbols;
           number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config.value =
            2;
        LOG_I(NR_MAC,
              "Setting TDD configuration slot_number %d is Mixed Slot %d slot_index %d gap_symbol \n",
              slot_number,
              slot_index,
              number_of_symbol);
      }

      for (int number_of_symbol = NR_NUMBER_OF_SYMBOLS_PER_SLOT - tdd_config->tdd_slot_bitmap[slot_index].num_ul_symbols;
           number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT;
           number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config.value =
            1;
        LOG_I(NR_MAC,
              "Setting TDD configuration slot_number %d is Mixed Slot %d slot_index %d ul_symbol \n",
              slot_number,
              slot_index,
              number_of_symbol);
      }
      LOG_I(NR_MAC, "Setting TDD configuration slot_number %d is MIXED %d slot_index \n", slot_number, slot_index);
      slot_number++;
    }

    if (tdd_config->tdd_slot_bitmap[slot_index].slot_type == TDD_NR_UPLINK_SLOT) {
      for (int number_of_symbol = 0; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config.value =
            1;
        LOG_I(NR_MAC,
              "Setting TDD configuration slot_number %d is UL Slot %d slot_index %d ul_symbol \n",
              slot_number,
              slot_index,
              number_of_symbol);
      }
      LOG_I(NR_MAC, "Setting TDD configuration slot_number %d is UL_Slot %d slot_index \n", slot_number, slot_index);
      slot_number++;
    }
  }
  return (tdd_config->tdd_numb_period_frame);
}

static void config_common(gNB_MAC_INST *nrmac, nr_pdsch_AntennaPorts_t pdsch_AntennaPorts, int pusch_AntennaPorts, NR_ServingCellConfigCommon_t *scc)
{
  nfapi_nr_config_request_scf_t *cfg = &nrmac->config[0];
  nrmac->common_channels[0].ServingCellConfigCommon = scc;

  // Carrier configuration
  struct NR_FrequencyInfoDL *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  cfg->carrier_config.dl_bandwidth.value =
      get_supported_bw_mhz(*frequencyInfoDL->frequencyBandList.list.array[0] > 256 ? FR2 : FR1,
                           frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                           frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.dl_bandwidth.tl.tag = NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG; // temporary
  cfg->num_tlv++;
  LOG_I(NR_MAC, "DL_Bandwidth:%d\n", cfg->carrier_config.dl_bandwidth.value);

  cfg->carrier_config.dl_frequency.value = from_nrarfcn(*frequencyInfoDL->frequencyBandList.list.array[0],
                                                        *scc->ssbSubcarrierSpacing,
                                                        frequencyInfoDL->absoluteFrequencyPointA)
                                            / 1000; // freq in kHz
  cfg->carrier_config.dl_frequency.tl.tag = NFAPI_NR_CONFIG_DL_FREQUENCY_TAG;
  cfg->num_tlv++;

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.dl_grid_size[i].value = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.dl_k0[i].value = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      cfg->carrier_config.dl_grid_size[i].tl.tag = NFAPI_NR_CONFIG_DL_GRID_SIZE_TAG;
      cfg->carrier_config.dl_k0[i].tl.tag = NFAPI_NR_CONFIG_DL_K0_TAG;
      cfg->num_tlv++;
      cfg->num_tlv++;
    } else {
      cfg->carrier_config.dl_grid_size[i].value = 0;
      cfg->carrier_config.dl_k0[i].value = 0;
    }
  }
  struct NR_FrequencyInfoUL *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  cfg->carrier_config.uplink_bandwidth.value =
      get_supported_bw_mhz(*frequencyInfoUL->frequencyBandList->list.array[0] > 256 ? FR2 : FR1,
                           frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                           frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.uplink_bandwidth.tl.tag = NFAPI_NR_CONFIG_UPLINK_BANDWIDTH_TAG; // temporary
  cfg->num_tlv++;
  LOG_I(NR_MAC, "DL_Bandwidth:%d\n", cfg->carrier_config.uplink_bandwidth.value);

  int UL_pointA;
  if (frequencyInfoUL->absoluteFrequencyPointA == NULL)
    UL_pointA = frequencyInfoDL->absoluteFrequencyPointA;
  else
    UL_pointA = *frequencyInfoUL->absoluteFrequencyPointA;

  cfg->carrier_config.uplink_frequency.value = from_nrarfcn(*frequencyInfoUL->frequencyBandList->list.array[0],
                                                            *scc->ssbSubcarrierSpacing,
                                                            UL_pointA)
                                               / 1000; // freq in kHz
  cfg->carrier_config.uplink_frequency.tl.tag = NFAPI_NR_CONFIG_UPLINK_FREQUENCY_TAG;
  cfg->num_tlv++;

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.ul_grid_size[i].value = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.ul_k0[i].value = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      cfg->carrier_config.ul_grid_size[i].tl.tag = NFAPI_NR_CONFIG_UL_GRID_SIZE_TAG;
      cfg->carrier_config.ul_k0[i].tl.tag = NFAPI_NR_CONFIG_UL_K0_TAG;
      cfg->num_tlv++;
      cfg->num_tlv++;
    } else {
      cfg->carrier_config.ul_grid_size[i].value = 0;
      cfg->carrier_config.ul_k0[i].value = 0;
    }
  }

  uint32_t band = *frequencyInfoDL->frequencyBandList.list.array[0];
  frequency_range_t frequency_range = band < 100 ? FR1 : FR2;

  frame_type_t frame_type = get_frame_type(*frequencyInfoDL->frequencyBandList.list.array[0], *scc->ssbSubcarrierSpacing);
  nrmac->common_channels[0].frame_type = frame_type;

  // Cell configuration
  cfg->cell_config.phy_cell_id.value = *scc->physCellId;
  cfg->cell_config.phy_cell_id.tl.tag = NFAPI_NR_CONFIG_PHY_CELL_ID_TAG;
  cfg->num_tlv++;

  cfg->cell_config.frame_duplex_type.value = frame_type;
  cfg->cell_config.frame_duplex_type.tl.tag = NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG;
  cfg->num_tlv++;

  // SSB configuration
  cfg->ssb_config.ss_pbch_power.value = scc->ss_PBCH_BlockPower;
  cfg->ssb_config.ss_pbch_power.tl.tag = NFAPI_NR_CONFIG_SS_PBCH_POWER_TAG;
  cfg->num_tlv++;

  cfg->ssb_config.bch_payload.value = 1;
  cfg->ssb_config.bch_payload.tl.tag = NFAPI_NR_CONFIG_BCH_PAYLOAD_TAG;
  cfg->num_tlv++;

  cfg->ssb_config.scs_common.value = *scc->ssbSubcarrierSpacing;
  cfg->ssb_config.scs_common.tl.tag = NFAPI_NR_CONFIG_SCS_COMMON_TAG;
  cfg->num_tlv++;

  // PRACH configuration

  uint8_t nb_preambles = 64;
  NR_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup;
  if (rach_ConfigCommon->totalNumberOfRA_Preambles != NULL)
    nb_preambles = *rach_ConfigCommon->totalNumberOfRA_Preambles;

  cfg->prach_config.prach_sequence_length.value = rach_ConfigCommon->prach_RootSequenceIndex.present - 1;
  cfg->prach_config.prach_sequence_length.tl.tag = NFAPI_NR_CONFIG_PRACH_SEQUENCE_LENGTH_TAG;
  cfg->num_tlv++;

  if (rach_ConfigCommon->msg1_SubcarrierSpacing)
    cfg->prach_config.prach_sub_c_spacing.value = *rach_ConfigCommon->msg1_SubcarrierSpacing;
  else
    cfg->prach_config.prach_sub_c_spacing.value = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing;
  cfg->prach_config.prach_sub_c_spacing.tl.tag = NFAPI_NR_CONFIG_PRACH_SUB_C_SPACING_TAG;
  cfg->num_tlv++;
  cfg->prach_config.restricted_set_config.value = rach_ConfigCommon->restrictedSetConfig;
  cfg->prach_config.restricted_set_config.tl.tag = NFAPI_NR_CONFIG_RESTRICTED_SET_CONFIG_TAG;
  cfg->num_tlv++;
  cfg->prach_config.prach_ConfigurationIndex.value = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
  cfg->prach_config.prach_ConfigurationIndex.tl.tag = NFAPI_NR_CONFIG_PRACH_CONFIG_INDEX_TAG;
  cfg->num_tlv++;

  switch (rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM) {
    case 0:
      cfg->prach_config.num_prach_fd_occasions.value = 1;
      break;
    case 1:
      cfg->prach_config.num_prach_fd_occasions.value = 2;
      break;
    case 2:
      cfg->prach_config.num_prach_fd_occasions.value = 4;
      break;
    case 3:
      cfg->prach_config.num_prach_fd_occasions.value = 8;
      break;
    default:
      AssertFatal(1 == 0, "msg1 FDM identifier %ld undefined (0,1,2,3) \n", rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM);
  }
  cfg->prach_config.num_prach_fd_occasions.tl.tag = NFAPI_NR_CONFIG_NUM_PRACH_FD_OCCASIONS_TAG;
  cfg->num_tlv++;

  cfg->prach_config.prach_ConfigurationIndex.value = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
  cfg->prach_config.prach_ConfigurationIndex.tl.tag = NFAPI_NR_CONFIG_PRACH_CONFIG_INDEX_TAG;
  cfg->num_tlv++;

  cfg->prach_config.num_prach_fd_occasions_list = (nfapi_nr_num_prach_fd_occasions_t *)malloc(
       cfg->prach_config.num_prach_fd_occasions.value * sizeof(nfapi_nr_num_prach_fd_occasions_t));
  for (int i = 0; i < cfg->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &cfg->prach_config.num_prach_fd_occasions_list[i];
    // prach_fd_occasion->num_prach_fd_occasions = i;
    if (cfg->prach_config.prach_sequence_length.value)
      prach_fd_occasion->prach_root_sequence_index.value = rach_ConfigCommon->prach_RootSequenceIndex.choice.l139;
    else
      prach_fd_occasion->prach_root_sequence_index.value = rach_ConfigCommon->prach_RootSequenceIndex.choice.l839;
    prach_fd_occasion->prach_root_sequence_index.tl.tag = NFAPI_NR_CONFIG_PRACH_ROOT_SEQUENCE_INDEX_TAG;
    cfg->num_tlv++;
    prach_fd_occasion->k1.value =
        NRRIV2PRBOFFSET(scc->uplinkConfigCommon->initialUplinkBWP->genericParameters.locationAndBandwidth, MAX_BWP_SIZE)
        + rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart
        + (get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing.value,
                       frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing)
           * i);
    if (get_softmodem_params()->sa) {
      prach_fd_occasion->k1.value =
          NRRIV2PRBOFFSET(scc->uplinkConfigCommon->initialUplinkBWP->genericParameters.locationAndBandwidth, MAX_BWP_SIZE)
          + rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart
          + (get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing.value,
                         frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing)
             * i);
    } else {
      prach_fd_occasion->k1.value = rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart
                                    + (get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing.value,
                                                   frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing)
                                       * i);
    }
    prach_fd_occasion->k1.tl.tag = NFAPI_NR_CONFIG_K1_TAG;
    cfg->num_tlv++;
    prach_fd_occasion->prach_zero_corr_conf.value = rach_ConfigCommon->rach_ConfigGeneric.zeroCorrelationZoneConfig;
    prach_fd_occasion->prach_zero_corr_conf.tl.tag = NFAPI_NR_CONFIG_PRACH_ZERO_CORR_CONF_TAG;
    cfg->num_tlv++;
    prach_fd_occasion->num_root_sequences.value = compute_nr_root_seq(rach_ConfigCommon, nb_preambles, frame_type, frequency_range);
    prach_fd_occasion->num_root_sequences.tl.tag = NFAPI_NR_CONFIG_NUM_ROOT_SEQUENCES_TAG;
    cfg->num_tlv++;
    prach_fd_occasion->num_unused_root_sequences.tl.tag = NFAPI_NR_CONFIG_NUM_UNUSED_ROOT_SEQUENCES_TAG;
    prach_fd_occasion->num_unused_root_sequences.value = 0;
    cfg->num_tlv++;
  }

  cfg->prach_config.ssb_per_rach.value = rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present - 1;
  cfg->prach_config.ssb_per_rach.tl.tag = NFAPI_NR_CONFIG_SSB_PER_RACH_TAG;
  cfg->num_tlv++;

  // SSB Table Configuration

  cfg->ssb_table.ssb_offset_point_a.value =
       get_ssb_offset_to_pointA(*scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB,
                                scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencyPointA,
                                *scc->ssbSubcarrierSpacing,
                                frequency_range);
  cfg->ssb_table.ssb_offset_point_a.tl.tag = NFAPI_NR_CONFIG_SSB_OFFSET_POINT_A_TAG;
  cfg->num_tlv++;
  cfg->ssb_table.ssb_period.value = *scc->ssb_periodicityServingCell;
  cfg->ssb_table.ssb_period.tl.tag = NFAPI_NR_CONFIG_SSB_PERIOD_TAG;
  cfg->num_tlv++;
  cfg->ssb_table.ssb_subcarrier_offset.value =
       get_ssb_subcarrier_offset(*scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB,
                                 scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencyPointA,
                                 *scc->ssbSubcarrierSpacing);
  cfg->ssb_table.ssb_subcarrier_offset.tl.tag = NFAPI_NR_CONFIG_SSB_SUBCARRIER_OFFSET_TAG;
  cfg->num_tlv++;

  uint8_t *mib_payload = nrmac->common_channels[0].MIB_pdu;
  uint32_t mib = (mib_payload[2] << 16) | (mib_payload[1] << 8) | mib_payload[0];
  cfg->ssb_table.MIB.tl.tag = NFAPI_NR_CONFIG_MIB_TAG;
  cfg->ssb_table.MIB.value = mib;
  cfg->num_tlv++;

  nrmac->ssb_SubcarrierOffset = cfg->ssb_table.ssb_subcarrier_offset.value;
  nrmac->ssb_OffsetPointA = cfg->ssb_table.ssb_offset_point_a.value;
  LOG_I(NR_MAC,
        "ssb_OffsetPointA %d, ssb_SubcarrierOffset %d\n",
        cfg->ssb_table.ssb_offset_point_a.value,
        cfg->ssb_table.ssb_subcarrier_offset.value);

  switch (scc->ssb_PositionsInBurst->present) {
    case 1:
      cfg->ssb_table.ssb_mask_list[0].ssb_mask.value = scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] << 24;
      cfg->ssb_table.ssb_mask_list[1].ssb_mask.value = 0;
      break;
    case 2:
      cfg->ssb_table.ssb_mask_list[0].ssb_mask.value = ((uint32_t)scc->ssb_PositionsInBurst->choice.mediumBitmap.buf[0]) << 24;
      cfg->ssb_table.ssb_mask_list[1].ssb_mask.value = 0;
      break;
    case 3:
      cfg->ssb_table.ssb_mask_list[0].ssb_mask.value = 0;
      cfg->ssb_table.ssb_mask_list[1].ssb_mask.value = 0;
      for (int i = 0; i < 4; i++) {
        cfg->ssb_table.ssb_mask_list[0].ssb_mask.value += (uint32_t)scc->ssb_PositionsInBurst->choice.longBitmap.buf[3 - i]
                                                          << i * 8;
        cfg->ssb_table.ssb_mask_list[1].ssb_mask.value += (uint32_t)scc->ssb_PositionsInBurst->choice.longBitmap.buf[7 - i]
                                                          << i * 8;
      }
      break;
    default:
      AssertFatal(1 == 0, "SSB bitmap size value %d undefined (allowed values 1,2,3) \n", scc->ssb_PositionsInBurst->present);
  }

  cfg->ssb_table.ssb_mask_list[0].ssb_mask.tl.tag = NFAPI_NR_CONFIG_SSB_MASK_TAG;
  cfg->ssb_table.ssb_mask_list[1].ssb_mask.tl.tag = NFAPI_NR_CONFIG_SSB_MASK_TAG;
  cfg->num_tlv += 2;

  // logical antenna ports
  int num_pdsch_antenna_ports = pdsch_AntennaPorts.N1 * pdsch_AntennaPorts.N2 * pdsch_AntennaPorts.XP;
  cfg->carrier_config.num_tx_ant.value = num_pdsch_antenna_ports;
  AssertFatal(num_pdsch_antenna_ports > 0 && num_pdsch_antenna_ports < 33, "pdsch_AntennaPorts in 1...32\n");
  cfg->carrier_config.num_tx_ant.tl.tag = NFAPI_NR_CONFIG_NUM_TX_ANT_TAG;

  int num_ssb = 0;
  for (int i = 0; i < 32; i++) {
    cfg->ssb_table.ssb_beam_id_list[i].beam_id.tl.tag = NFAPI_NR_CONFIG_BEAM_ID_TAG;
    if ((cfg->ssb_table.ssb_mask_list[0].ssb_mask.value >> (31 - i)) & 1) {
      cfg->ssb_table.ssb_beam_id_list[i].beam_id.value = num_ssb;
      num_ssb++;
    }
    cfg->num_tlv++;
  }
  for (int i = 0; i < 32; i++) {
    cfg->ssb_table.ssb_beam_id_list[32 + i].beam_id.tl.tag = NFAPI_NR_CONFIG_BEAM_ID_TAG;
    if ((cfg->ssb_table.ssb_mask_list[1].ssb_mask.value >> (31 - i)) & 1) {
      cfg->ssb_table.ssb_beam_id_list[32 + i].beam_id.value = num_ssb;
      num_ssb++;
    }
    cfg->num_tlv++;
  }

  cfg->carrier_config.num_rx_ant.value = pusch_AntennaPorts;
  AssertFatal(pusch_AntennaPorts > 0 && pusch_AntennaPorts < 13, "pusch_AntennaPorts in 1...12\n");
  cfg->carrier_config.num_rx_ant.tl.tag = NFAPI_NR_CONFIG_NUM_RX_ANT_TAG;
  LOG_I(NR_MAC,
        "Set RX antenna number to %d, Set TX antenna number to %d (num ssb %d: %x,%x)\n",
        cfg->carrier_config.num_tx_ant.value,
        cfg->carrier_config.num_rx_ant.value,
        num_ssb,
        cfg->ssb_table.ssb_mask_list[0].ssb_mask.value,
        cfg->ssb_table.ssb_mask_list[1].ssb_mask.value);
  AssertFatal(cfg->carrier_config.num_tx_ant.value > 0,
              "carrier_config.num_tx_ant.value %d !\n",
              cfg->carrier_config.num_tx_ant.value);
  cfg->num_tlv++;
  cfg->num_tlv++;

  // TDD Table Configuration
  if (cfg->cell_config.frame_duplex_type.value == TDD) {
    nrmac->tdd_config.is_tdd = true;
    cfg->tdd_table.tdd_period.tl.tag = NFAPI_NR_CONFIG_TDD_PERIOD_TAG;
    cfg->num_tlv++;
    int num_tdd_patterns = get_tdd_period(scc->tdd_UL_DL_ConfigurationCommon, cfg, &nrmac->tdd_config);
    int periods_per_frame = 0;

    LOG_I(NR_MAC,
          "Setting TDD configuration period to %d, num_tdd_patterns %d\n",
          cfg->tdd_table.tdd_period.value,
          num_tdd_patterns);

    periods_per_frame =
        set_tdd_config_nr_new(cfg, frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing, &nrmac->tdd_config);

    if (frequency_range == FR2) {
      LOG_I(NR_MAC, "Configuring TDD beam association to default\n");
      nrmac->tdd_beam_association = malloc16(periods_per_frame * sizeof(int16_t));
      for (int i = 0; i < periods_per_frame; ++i)
        nrmac->tdd_beam_association[i] = -1; /* default: beams not configured */
    } else {
      nrmac->tdd_beam_association = NULL; /* default: no beams */
    }
  }

  // precoding matrix configuration (to be improved)
  cfg->pmi_list = init_DL_MIMO_codebook(nrmac, pdsch_AntennaPorts);
}

void nr_mac_config_scc(gNB_MAC_INST *nrmac, NR_ServingCellConfigCommon_t *scc, const nr_mac_config_t *config)
{
  DevAssert(nrmac != NULL);
  DevAssert(scc != NULL);
  DevAssert(config != NULL);

  AssertFatal(scc->ssb_PositionsInBurst->present > 0 && scc->ssb_PositionsInBurst->present < 4,
              "SSB Bitmap type %d is not valid\n",
              scc->ssb_PositionsInBurst->present);

  const int NTN_gNB_Koffset = get_NTN_Koffset(scc);
  const int n = nr_slots_per_frame[*scc->ssbSubcarrierSpacing];
  const int size = n << (int)ceil(log2((NTN_gNB_Koffset + 13) / n + 1)); // 13 is upper limit for max_fb_time

  nrmac->vrb_map_UL_size = size;
  nrmac->common_channels[0].vrb_map_UL = calloc(size * MAX_BWP_SIZE, sizeof(uint16_t));
  AssertFatal(nrmac->common_channels[0].vrb_map_UL,
              "could not allocate memory for RC.nrmac[]->common_channels[0].vrb_map_UL\n");

  nrmac->UL_tti_req_ahead_size = size;
  nrmac->UL_tti_req_ahead[0] = calloc(size, sizeof(nfapi_nr_ul_tti_request_t));
  AssertFatal(nrmac->UL_tti_req_ahead[0], "could not allocate memory for nrmac->UL_tti_req_ahead[0]\n");

  LOG_I(NR_MAC, "Configuring common parameters from NR ServingCellConfig\n");

  config_common(nrmac, config->pdsch_AntennaPorts, config->pusch_AntennaPorts, scc);

  if (NFAPI_MODE == NFAPI_MONOLITHIC) {
    // nothing to be sent in the other cases
    NR_PHY_Config_t phycfg = {.Mod_id = 0, .CC_id = 0, .cfg = &nrmac->config[0]};
    DevAssert(nrmac->if_inst->NR_PHY_config_req);
    nrmac->if_inst->NR_PHY_config_req(&phycfg);
  }

  find_SSB_and_RO_available(nrmac);

  const NR_TDD_UL_DL_Pattern_t *tdd = scc->tdd_UL_DL_ConfigurationCommon ? &scc->tdd_UL_DL_ConfigurationCommon->pattern1 : NULL;

  int nr_slots_period = n;
  if (tdd) {
    nr_slots_period = nrmac->tdd_config.tdd_numb_slots_period;
  } else {
    // if TDD configuration is not present and the band is not FDD, it means it is a dynamic TDD configuration
    AssertFatal(nrmac->common_channels[0].frame_type == FDD,"Dynamic TDD not handled yet\n");
  }

  for (int slot = 0; slot < n; ++slot) {
    nrmac->dlsch_slot_bitmap[slot / 64] |= (uint64_t)(is_dl_slot(slot % nr_slots_period, nrmac->tdd_config.tdd_slot_bitmap))
                                           << (slot % 64);
    nrmac->ulsch_slot_bitmap[slot / 64] |= (uint64_t)(is_ul_slot(slot % nr_slots_period, nrmac->tdd_config.tdd_slot_bitmap))
                                           << (slot % 64);

    LOG_D(NR_MAC,
          "slot %d DL %d UL %d\n",
          slot,
          (nrmac->dlsch_slot_bitmap[slot / 64] & ((uint64_t)1 << (slot % 64))) != 0,
          (nrmac->ulsch_slot_bitmap[slot / 64] & ((uint64_t)1 << (slot % 64))) != 0);
  }

  if (get_softmodem_params()->phy_test) {
    nrmac->pre_processor_dl = nr_preprocessor_phytest;
    nrmac->pre_processor_ul = nr_ul_preprocessor_phytest;
  } else {
    nrmac->pre_processor_dl = nr_init_fr1_dlsch_preprocessor(0);
    nrmac->pre_processor_ul = nr_init_fr1_ulsch_preprocessor(0);
  }

  NR_COMMON_channels_t *cc = &nrmac->common_channels[0];
  NR_SCHED_LOCK(&nrmac->sched_lock);
  for (int n = 0; n < NR_NB_RA_PROC_MAX; n++) {
    NR_RA_t *ra = &cc->ra[n];
    nr_clear_ra_proc(ra);
  }
  NR_SCHED_UNLOCK(&nrmac->sched_lock);
}

void nr_mac_configure_sib1(gNB_MAC_INST *nrmac, const f1ap_plmn_t *plmn, uint64_t cellID, int tac)
{
  AssertFatal(get_softmodem_params()->sa > 0, "error: SIB1 only applicable for SA\n");

  NR_COMMON_channels_t *cc = &nrmac->common_channels[0];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  NR_BCCH_DL_SCH_Message_t *sib1 = get_SIB1_NR(scc, plmn, cellID, tac, &nrmac->radio_config.timer_config);
  cc->sib1 = sib1;
  cc->sib1_bcch_length = encode_SIB1_NR(sib1, cc->sib1_bcch_pdu, sizeof(cc->sib1_bcch_pdu));
  LOG_E(NR_MAC, "----------------- ASN1 ENCODER PRINT START ----------------- \n");
  xer_fprint(stdout, &asn_DEF_NR_BCCH_DL_SCH_Message, sib1);
  LOG_E(NR_MAC, "----------------- ASN1 ENCODER PRINT END----------------- \n");
  AssertFatal(cc->sib1_bcch_length > 0, "could not encode SIB1\n");
}

bool nr_mac_add_test_ue(gNB_MAC_INST *nrmac, uint32_t rnti, NR_CellGroupConfig_t *CellGroup)
{
  /* ideally, instead of this function, "users" of this function should call
   * the ue context setup request function in mac_rrc_dl_handler.c */
  DevAssert(nrmac != NULL);
  DevAssert(CellGroup != NULL);
  DevAssert(get_softmodem_params()->phy_test);
  NR_SCHED_LOCK(&nrmac->sched_lock);

  NR_UE_info_t *UE = add_new_nr_ue(nrmac, rnti, CellGroup);
  if (!UE) {
    LOG_E(NR_MAC, "Error adding UE %04x\n", rnti);
    NR_SCHED_UNLOCK(&nrmac->sched_lock);
    return false;
  }

  if (CellGroup->spCellConfig && CellGroup->spCellConfig->reconfigurationWithSync
      && CellGroup->spCellConfig->reconfigurationWithSync->rach_ConfigDedicated
      && CellGroup->spCellConfig->reconfigurationWithSync->rach_ConfigDedicated->choice.uplink->cfra) {
    nr_mac_prepare_ra_ue(RC.nrmac[0], UE->rnti, CellGroup);
  }
  process_addmod_bearers_cellGroupConfig(&UE->UE_sched_ctrl, CellGroup->rlc_BearerToAddModList);
  AssertFatal(CellGroup->rlc_BearerToReleaseList == NULL, "cannot release bearers while adding new UEs\n");
  NR_SCHED_UNLOCK(&nrmac->sched_lock);
  LOG_I(NR_MAC, "Added new UE %x with initial CellGroup\n", rnti);
  return true;
}

bool nr_mac_prepare_ra_ue(gNB_MAC_INST *nrmac, uint32_t rnti, NR_CellGroupConfig_t *CellGroup)
{
  DevAssert(nrmac != NULL);
  DevAssert(CellGroup != NULL);
  NR_SCHED_ENSURE_LOCKED(&nrmac->sched_lock);

  // NSA case: need to pre-configure CFRA
  const int CC_id = 0;
  NR_COMMON_channels_t *cc = &nrmac->common_channels[CC_id];
  uint8_t ra_index = 0;
  /* checking for free RA process */
  for(; ra_index < NR_NB_RA_PROC_MAX; ra_index++) {
    if ((cc->ra[ra_index].ra_state == nrRA_gNB_IDLE) && (!cc->ra[ra_index].cfra))
      break;
  }
  if (ra_index == NR_NB_RA_PROC_MAX) {
    LOG_E(NR_MAC, "RA processes are not available for CFRA RNTI %04x\n", rnti);
    return false;
  } 
  NR_RA_t *ra = &cc->ra[ra_index];
  ra->cfra = true;
  ra->rnti = rnti;
  ra->CellGroup = CellGroup;
  struct NR_CFRA *cfra = CellGroup->spCellConfig->reconfigurationWithSync->rach_ConfigDedicated->choice.uplink->cfra;
  uint8_t num_preamble = cfra->resources.choice.ssb->ssb_ResourceList.list.count;
  ra->preambles.num_preambles = num_preamble;
  for (int i = 0; i < cc->num_active_ssb; i++) {
    for (int j = 0; j < num_preamble; j++) {
      if (cc->ssb_index[i] == cfra->resources.choice.ssb->ssb_ResourceList.list.array[j]->ssb) {
        // one dedicated preamble for each beam
        ra->preambles.preamble_list[i] = cfra->resources.choice.ssb->ssb_ResourceList.list.array[j]->ra_PreambleIndex;
        break;
      }
    }
  }
  LOG_I(NR_MAC, "Added new %s process for UE RNTI %04x with initial CellGroup with RA Index %u\n", ra->cfra ? "CFRA" : "CBRA", rnti, ra_index);
  return true;
}

/* Prepare a new CellGroupConfig to be applied for this UE. We cannot
 * immediatly apply it, as we have to wait for the reconfiguration through RRC.
 * This function sets up everything to apply the reconfiguration. Later, we
 * will trigger the timer with nr_mac_enable_ue_rrc_processing_timer(); upon
 * expiry nr_mac_apply_cellgroup() will apply the CellGroupConfig (radio config
 * etc). */
bool nr_mac_prepare_cellgroup_update(gNB_MAC_INST *nrmac, NR_UE_info_t *UE, NR_CellGroupConfig_t *CellGroup)
{
  DevAssert(nrmac != NULL);
  DevAssert(UE != NULL);
  DevAssert(CellGroup != NULL);

  /* we assume that this function is mutex-protected from outside */
  NR_SCHED_ENSURE_LOCKED(&nrmac->sched_lock);

  UE->reconfigCellGroup = CellGroup;
  UE->expect_reconfiguration = true;

  return true;
}
