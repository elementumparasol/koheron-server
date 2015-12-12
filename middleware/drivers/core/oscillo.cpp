/// (c) Koheron

#include "oscillo.hpp"

#include <thread>
#include <chrono>

Oscillo::Oscillo(Klib::DevMem& dev_mem_)
: dev_mem(dev_mem_)
, data(0)
, data_all(0)
{
    avg_on = false;
    
    waveform_size = 0;
    status = CLOSED;
}
 
Oscillo::~Oscillo()
{
    Close();
}

int Oscillo::Open(uint32_t const_ip_addr, uint32_t adc_1_addr, uint32_t adc_2_addr, uint32_t waveform_size_)
{
    // Reopening
    if(status == OPENED && waveform_size_ != waveform_size) {
        Close();
    }

    if(status == CLOSED) {
        waveform_size = waveform_size_;
        
        // Acquisition time in microseconds
        // Factor two because depending whether TRIG_ACQ
        // is received at the beginning or the end of a
        // period the acquisition time can be twice as long
        acq_time_us = 2*(waveform_size*1E6)/SAMPLING_RATE;
    
        const_ip_map = dev_mem.AddMemoryMap(const_ip_addr, 16*MAP_SIZE);
        
        if(static_cast<int>(const_ip_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_1_map = dev_mem.AddMemoryMap(adc_1_addr,
                                         (waveform_size/1024)*MAP_SIZE);
        
        if(static_cast<int>(adc_1_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        adc_2_map = dev_mem.AddMemoryMap(adc_2_addr,
                                         (waveform_size/1024)*MAP_SIZE);
        
        if(static_cast<int>(adc_2_map) < 0) {
            status = FAILED;
            return -1;
        }
        
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_START_OFFSET,0);
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_ACQ_OFFSET, 0);
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_START_OFFSET,1);
        
        data = Klib::KVector<float>(waveform_size, 0);
        data_all = Klib::KVector<float>(2*waveform_size, 0);
        
        status = OPENED;
        
        // Reset averaging
        set_averaging(false);
    }
    
    return 0;
}

void Oscillo::Close()
{
    if(status == OPENED) {
        dev_mem.RmMemoryMap(const_ip_map);
        dev_mem.RmMemoryMap(adc_1_map);
        dev_mem.RmMemoryMap(adc_2_map);
        status = CLOSED;
    }
}

void Oscillo::_wait_for_acquisition()
{
    // The overhead of sleep_for might be of the order of our waiting time:
    // http://stackoverflow.com/questions/18071664/stdthis-threadsleep-for-and-nanoseconds
    std::this_thread::sleep_for(std::chrono::microseconds(acq_time_us));
}

// http://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c
long long int mod(long long int k, long long int n)
{
    return ((k %= n) < 0) ? k+n : k;
}

#define POW_2_31 2147483648 // 2^31
#define POW_2_32 4294967296 // 2^32

float _raw_to_float(uint32_t raw)
{
    return float(mod(raw - POW_2_31, POW_2_32) - POW_2_31);
}

void Oscillo::_raw_to_vector(uint32_t *raw_data)
{    
    if(avg_on) {
        uint32_t num_avg 
            = Klib::ReadReg32(dev_mem.GetBaseAddr(const_ip_map)+NUM_AVG_OFFSET);
    
        for(unsigned int i=0; i<data.size(); i++) {
            data[i] = _raw_to_float(raw_data[i]) / float(num_avg);
        }
    } else {
        for(unsigned int i=0; i<data.size(); i++) {
            data[i] = _raw_to_float(raw_data[i]);
        }
    }
}

void Oscillo::_raw_to_vector_all(uint32_t *raw_data_1, uint32_t *raw_data_2)
{    
    if(avg_on) {
        uint32_t num_avg 
            = Klib::ReadReg32(dev_mem.GetBaseAddr(const_ip_map)+NUM_AVG_OFFSET);
    
        for(unsigned int i=0; i<waveform_size; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]) / float(num_avg);
            data_all[i + waveform_size] 
                = _raw_to_float(raw_data_2[i]) / float(num_avg);
        }
    } else {
        for(unsigned int i=0; i<waveform_size; i++) {
            data_all[i] = _raw_to_float(raw_data_1[i]);
            data_all[i + waveform_size] = _raw_to_float(raw_data_2[i]);
        }
    }
}

Klib::KVector<float>& Oscillo::read_data(bool channel)
{
    Klib::MemMapID adc_map;
    channel ? adc_map = adc_1_map : adc_map = adc_2_map;

    Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_ACQ_OFFSET, 1);
    
    _wait_for_acquisition();
    
    uint32_t *raw_data 
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_map));
    _raw_to_vector(raw_data);

    Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_ACQ_OFFSET, 0);
    return data;
}

Klib::KVector<float>& Oscillo::read_all_channels()
{
    Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_ACQ_OFFSET, 1);
    
    _wait_for_acquisition();
    
    uint32_t *raw_data_1
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_1_map));
    uint32_t *raw_data_2
        = reinterpret_cast<uint32_t*>(dev_mem.GetBaseAddr(adc_2_map));
        
    _raw_to_vector_all(raw_data_1, raw_data_2);

    Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+TRIG_ACQ_OFFSET, 0);
    return data_all;
}

void Oscillo::set_averaging(bool avg_status)
{
    avg_on = avg_status;
    
    if(avg_on) {
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+AVG_ON_1_OFFSET, 1);
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+AVG_ON_2_OFFSET, 1);
    } else {
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+AVG_ON_1_OFFSET, 0);
        Klib::WriteReg32(dev_mem.GetBaseAddr(const_ip_map)+AVG_ON_2_OFFSET, 0);
    }
}

uint32_t Oscillo::get_num_average()
{
    return avg_on ? Klib::ReadReg32(dev_mem.GetBaseAddr(const_ip_map)+NUM_AVG_OFFSET) : 0;
}
