/**
 * @file
 */
#ifndef __KFIFO_H__
#define __KFIFO_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct kfifo {
    int start;
    int end;
    int count;
    void *data;
    size_t size;
    size_t esize;
    size_t header_size;
    size_t payload_size;
};

/**
 * @brief  Initialize a FIFO using preallocated buffer
 * @param  fifo: The fifo object.
 * @param  data: The data space for the FIFO.
 * @param  esize: Element size of the FIFO.
 * @param  size: Number of elements in the FIFO.
 * @retval None
 */
void kfifo_init(struct kfifo *fifo, void *data, size_t esize, size_t size);

/**
 * @brief  Dynamically allocate a new fifo buffer
 * @param  esize: Element size of the FIFO.
 * @param  size: Number of elements in the FIFO.
 * @retval kfifo: Returning object of the allocated FIFO.
 * @retval None
 */
struct kfifo *kfifo_alloc(size_t nmem, size_t size);

/**
 * @brief  Deallocate the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval None
 */
void kfifo_free(struct kfifo *fifo);

/**
 * @brief  Put data into the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @param  data: Pointer to the data.
 * @param  n: Size of the data in bytes.
 * @retval None
 */
void kfifo_in(struct kfifo *fifo, const void *data, size_t n);

/**
 * @brief  Get data from the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @param  data: The memory space for retrieving data.
 * @param  n: Size of the data buffer.
 * @retval None
 */
void kfifo_out(struct kfifo *fifo, void *data, size_t n);

/**
 * @brief  Get some data from the FIFO without removing it
 * @param  fifo: Pointer to the FIFO.
 * @param  data: The memory space for retrieving data.
 * @param  n: Size of the data buffer.
 * @retval None
 */
void kfifo_out_peek(struct kfifo *fifo, void *data, size_t n);

/**
 * @brief  Read data pointer of the next element to write
 * @param  fifo: Pointer to the FIFO.
 * @param  data_ptr: For saving the data pointer of the next FIFO
 *         element to write with DMA.
 * @retval None
 */
void kfifo_dma_in_prepare(struct kfifo *fifo, char **data_ptr);

/**
 * @brief  Complete data writing of FIFO with DMA
 * @param  fifo: The fifo object to provide.
 * @param  n: Size of the data written to the FIFO in bytes.
 * @retval None
 */
void kfifo_dma_in_finish(struct kfifo *fifo, size_t n);

/**
 * @brief  Read data pointer of the next FIFO element to read
 * @param  fifo: Pointer to the FIFO.
 * @param  data_ptr: For saving the data pointer of the next FIFO
 *         element to read with DMA.
 * @param  n: Size of the data buffer to read from the fifo with DMA
 *         in bytes.
 * @retval None
 */
void kfifo_dma_out_prepare(struct kfifo *fifo, char **data_ptr, size_t *n);

/**
 * @brief  Complete data reading of the FIFO with DMA
 * @param  fifo: Pointer to the FIFO.
 * @retval None
 */
void kfifo_dma_out_finish(struct kfifo *fifo);

/**
 * @brief  Put data into the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @param  data: Pointer to the data.
 * @retval None
 */
void kfifo_put(struct kfifo *fifo, void *data);

/**
 * @brief  Get data from the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @param  data: Pointer to the memory space for retrieving data.
 * @retval None
 */
void kfifo_get(struct kfifo *fifo, void *data);

/**
 * @brief  Get data from the FIFO without removing it.
 * @param  fifo: Pointer to the FIFO.
 * @param  data: Pointer to the memory space for retrieving data.
 * @retval None
 */
void kfifo_peek(struct kfifo *fifo, void *data);

/**
 * @brief  Return the record length of the next FIFO element to read
 * @param  fifo: Pointer to the memory space for retrieving data.
 * @retval size_t: Size of the next FIFO element in bytes.
 */
size_t kfifo_peek_len(struct kfifo *fifo);

/**
 * @brief  Skip next data to read from the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval None
 */
void kfifo_skip(struct kfifo *fifo);

/**
 * @brief  Return the number of unused elements (free space) in the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval size_t: The number of the avaliable slots.
 */
size_t kfifo_avail(struct kfifo *fifo);

/**
 * @brief  Return the number of used elements in the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval size_t: The number of used elements can be read.
 */
size_t kfifo_len(struct kfifo *fifo);

/**
 * @brief  Return the size of the element managed by the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval size_t: The size of the element managed by the FIFO.
 */
size_t kfifo_esize(struct kfifo *fifo);

/**
 * @brief  Return the number of elements can be stored in the FIFO
 * @param  fifo: Pointer to the FIFO.
 * @retval size_t: The number of elements can be stored in the FIFO.
 */
size_t kfifo_size(struct kfifo *fifo);

/**
 * @brief  Return the size of the kfifo header
 * @param  None
 * @retval size_t: The size of the kfifo header.
 */
size_t kfifo_header_size(void);

/**
 * @brief  Return if the FIFO is empty or not
 * @param  fifo: Pointer to the FIFO.
 * @retval bool: true or false.
 */
bool kfifo_is_empty(struct kfifo *fifo);

/**
 * @brief  Return if the FIFO is full or not
 * @param  fifo: Pointer to the FIFO.
 * @retval bool: true or false.
 */
bool kfifo_is_full(struct kfifo *fifo);

#endif
