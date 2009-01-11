#ifndef DPS_IO_H
#define DPS_IO_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/*!
 * @brief DPS opened file description struct
 *
 * @{
 */
struct dps_info
{
    /** opened file pointer */
    FILE* res;

    /** field per frame number, currently 2 by default */
    size_t fields_count;

    /** frames count */
    size_t frames_count;

    /** index data: file offsets of frames/fields data */
    size_t* frames_pos;

    /** index data: sizes of frame/fields data */
    int32_t* frames_size;
};
/** @} */


/*!
 * @anchor dps_open @name dps_open
 *
 * Open DPS file, parse 
 *
 * @param[in] dps pointer to clean dps_info struct
 * @param[in] filename DPS file name to open
 *
 * @return 0 on success, otherwise negative errono value
 *
 * @{
 */
int dps_open(struct dps_info* dps, char* filename);
/** @} */

/*!
 * @anchor dps_close @name dps_close
 *
 * Close DPS file, release all index tables
 *
 * @param[in] dps pointer to dps_info struct
 *
 * @return 0
 *
 * @{
 */
int dps_close(struct dps_info* dps);
/** @} */

/*!
 * @brief DPS frame info
 *
 * @{
 */
struct dps_frame
{
    /** frame/field number */
    size_t num;

    /** data pointer */
    void* data;

    /** frame data size */
    size_t size;
};
/** @} */

/*!
 * @anchor dps_frame_read @name dps_frame_read
 *
 * Read DPS frame/field data
 *
 * @param[in] dps pointer to dps_info struct
 * @param[in] num frame/field number
 *
 * @return poiter to frame on success, otherwise NULL
 *
 * @{
 */
struct dps_frame* dps_frame_read(struct dps_info* dps, size_t num);
/** @} */

/*!
 * @anchor dps_frame_release @name dps_frame_release
 *
 * Free data of DPS frame struct
 *
 * @param[in] pointer to frame/field data struct
 *
 * @{
 */
void dps_frame_release(struct dps_frame* frame);
/** @} */

#ifdef __cplusplus
};
#endif /* __cplusplus */


#endif /* DPS_IO_H */
