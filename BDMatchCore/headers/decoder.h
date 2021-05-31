﻿#pragma once
#include "datastruct.h"
#include <memory>
#include <stop_token>
#include "language_pack.h"
extern"C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include "libswscale/swscale.h"
#include <libavutil/channel_layout.h>
#include "libavutil/md5.h"
#include "libavutil/opt.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include <libswresample/swresample.h>
#include <fftw3.h>
}

namespace Decode {

	typedef void(*prog_func)(int, double);

	template <typename T, int type>
	concept Normalizable = (type == 0 || (std::is_same<T, int>::value && type == 24)) && 
		requires (T v) {
			static_cast<double>(v);
			std::numeric_limits<T>::max();
		}
	;

	struct FFmpeg
	{
	public:
		~FFmpeg();
		AVFormatContext * filefm = nullptr;
		AVCodecContext *codecfm = nullptr;
		AVCodec *codec = nullptr;
		AVPacket *packet = nullptr;
		uint8_t **dst_data = nullptr;
		AVFrame *decoded_frame = nullptr;
		double **sample_seqs = nullptr;
		struct SwrContext *swr_ctx = nullptr;
		int real_ch = 0;
	};
	
	class Decode
	{
	public:
		Decode(Language_Pack& lang_pack0, std::stop_source &stop_src0);
		virtual ~Decode();
		int load_settings(const int &fft_num0, const bool &output_pcm0, const int &min_db0, 
			const int &resamp_rate0, const int &prog_type0,	fftw_plan plan0, const prog_func &prog_single0 = nullptr);
		int initialize(const std::string_view &file_name0);
		int decode_audio();
		std::string_view get_feedback();
		std::string_view get_file_name();
		int get_return();
		int64_t get_fft_samp_num();
		int64_t get_milisec();
		int get_channels();
		int get_samp_rate();
		int get_fft_num();
		bool get_audio_only();
		DataStruct::Spec_Node** get_fft_data();
		char** get_fft_spec();
		double get_avg_vol();
		int set_vol_mode(const int &input);
		int set_vol_coef(const double &input);
	protected:
		void sub_prog_back(int type, double val);
		int clear_fft_data();
		int clear_ffmpeg();
		int clear_normalized_samples(double** normalized_samples);
		template <typename T, int type = 0>
			requires Normalizable<T, type>
		int transfer_audio_data_planar(uint8_t** const audiodata, double** const normalized_samples, double** const seqs, 
			const int& nb_last, const int& nb_last_next, const int& length);
		template <typename T, int type = 0>
			requires Normalizable<T, type>
		int transfer_audio_data_packed(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& nb_last_next, const int& length);
		virtual int normalize(uint8_t ** const &audiodata, double ** &normalized_samples, double ** &seqs, 
			int &nb_last, const int &nb_samples);
		int FFT(DataStruct::Spec_Node** nodes, double** in, int fft_index, const int nb_fft);
		virtual int FD8(double* inseq, DataStruct::Spec_Node* outseq);
		std::stop_source& stop_src;//multithreading cancel source
		//language pack
		Language_Pack& lang_pack;
		//settings
		int fft_num = 512;
		int min_db = -14;
		bool output_pcm = false;
		int vol_mode = -1;
		int resamp_rate = 0;
		fftw_plan plan = nullptr;
		//fft data
		DataStruct::Spec_Node** fft_data = nullptr;
		char** fft_spec = nullptr;
		DataStruct::Spec_Node* fft_data_mem = nullptr;
		char* fft_spec_mem = nullptr;
		//audio info
		std::string_view file_name;
		int out_bit_depth = 0;
		int audio_stream = 0;
		int64_t milisec = 0;
		int64_t fft_samp_num = 0;
		int64_t e_fft_num = 0;
		int channels = 0;//audio channels
		int data_channels = 0;//audio data channels
		int real_ch = 0;//Planar: channels, Linear: 1
		int64_t start_time = 0;
		bool audio_only = false;
		//sample info
		int sample_type = 0;
		int sample_rate = 0;
		//progress bar and return
		prog_func prog_single = nullptr;//func_ptr for progress bar
		std::string feedback;
		int return_val = -100;
		int prog_type = 0;
		int decoded_num = 0;
		//fix audio volume
		double prog_val = 0.0;
		double total_vol = 0.0;
		double vol_coef = 0.0;
		double c_min_db = 0.0;
		//ffmpeg
		FFmpeg *ffmpeg = nullptr;
	};

	class Decode_SSE :public Decode {
	public:
		Decode_SSE(Language_Pack& lang_pack0, std::stop_source& stop_src0)
			:Decode(lang_pack0, stop_src0) {}
		virtual int FD8(double* inseq, DataStruct::Spec_Node* outseq);
	};

	class Decode_AVX :public Decode {
	public:
		Decode_AVX(Language_Pack& lang_pack0, std::stop_source& stop_src0)
			:Decode(lang_pack0, stop_src0) {}
		int transfer_audio_data_planar_float(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& nb_last_next, const int& length, const int& nb_samples);
		int transfer_audio_data_packed_float(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& length, const int& nb_samples);
		virtual int normalize(uint8_t** const& audiodata, double**& normalized_samples, double**& seqs,
			int& nb_last, const int& nb_samples);
		virtual int FD8(double* inseq, DataStruct::Spec_Node* outseq);
	};

	class Decode_AVX2 :public Decode_AVX {
	public:
		Decode_AVX2(Language_Pack& lang_pack0, std::stop_source& stop_src0)
			:Decode_AVX(lang_pack0, stop_src0) {}
		int transfer_audio_data_packed_int16(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& length, const int& nb_samples);
		int transfer_audio_data_packed_int24(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& length, const int& nb_samples);
		int transfer_audio_data_packed_int32(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
			const int& nb_last, const int& length, const int& nb_samples);
		virtual int normalize(uint8_t ** const &audiodata, double ** &normalized_samples, double ** &seqs,
			int &nb_last, const int &nb_samples);
		virtual int FD8(double* inseq, DataStruct::Spec_Node* outseq);
	};

	template<typename T, int type>
		requires Normalizable<T, type>
	inline int Decode::Decode::transfer_audio_data_planar(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length)
	{
		int index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int ch = 0; ch < channels; ch++) {
			T* tempT = reinterpret_cast<T*>(audiodata[ch]);
			int k = 0;
			for (int i = nb_last; i < length; i++, k++)normalized_samples[ch][i] = static_cast<double>(tempT[k]) / static_cast<double>(std::numeric_limits<T>::max());
			for (int i = index2; i < nb_last_next; i++, k++)seqs[ch][i] = static_cast<double>(tempT[k]) / static_cast<double>(std::numeric_limits<T>::max());
		}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_planar<double>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		int index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int ch = 0; ch < channels; ch++) {
			double* tempd = reinterpret_cast<double*>(audiodata[ch]);
			int k = 0;
			for (int i = nb_last; i < length; i++, k++)normalized_samples[ch][i] = tempd[k];
			for (int i = index2; i < nb_last_next; i++, k++)seqs[ch][i] = tempd[k];
		}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_planar<float>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		int index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int ch = 0; ch < channels; ch++) {
			float* tempf = reinterpret_cast<float*>(audiodata[ch]);
			int k = 0;
			for (int i = nb_last; i < length; i++, k++)normalized_samples[ch][i] = static_cast<double>(tempf[k]);
			for (int i = index2; i < nb_last_next; i++, k++)seqs[ch][i] = static_cast<double>(tempf[k]);
		}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_planar<int, 24>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		int index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int ch = 0; ch < channels; ch++) {
			int* tempi = reinterpret_cast<int*>(audiodata[ch]);
			int k = 0;
			for (int i = nb_last; i < length; i++, k++)normalized_samples[ch][i] = static_cast<double>(tempi[k] >> 8) / 8388607.0;
			for (int i = index2; i < nb_last_next; i++, k++)seqs[ch][i] = static_cast<double>(tempi[k] >> 8) / 8388607.0;
		}
		return 0;
	}

	template<typename T, int type>
		requires Normalizable<T, type>
	int inline Decode::Decode::transfer_audio_data_packed(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length)
	{
		T* tempT = reinterpret_cast<T*>(audiodata[0]);
		int index = 0, index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int i = nb_last; i < length; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				normalized_samples[ch][i] = static_cast<double>(tempT[index]) / static_cast<double>(std::numeric_limits<T>::max());
			}
		for (int i = index2; i < nb_last_next; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				seqs[ch][i] = static_cast<double>(tempT[index]) / static_cast<double>(std::numeric_limits<T>::max());
			}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_packed<float>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		float* tempf = reinterpret_cast<float*>(audiodata[0]);
		int index = 0, index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int i = nb_last; i < length; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				normalized_samples[ch][i] = static_cast<double>(tempf[index]);
			}
		for (int i = index2; i < nb_last_next; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				seqs[ch][i] = static_cast<double>(tempf[index]);
			}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_packed<double>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		double* tempd = reinterpret_cast<double*>(audiodata[0]);
		int index = 0, index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int i = nb_last; i < length; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				normalized_samples[ch][i] = tempd[index];
			}
		for (int i = index2; i < nb_last_next; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				seqs[ch][i] = tempd[index];
			}
		return 0;
	}
	template<> inline int Decode::Decode::transfer_audio_data_packed<int, 24>(uint8_t** const audiodata, double** const normalized_samples, double** const seqs,
		const int& nb_last, const int& nb_last_next, const int& length) {
		int* tempi = reinterpret_cast<int*>(audiodata[0]);
		int index = 0, index2 = 0;
		if (length > 0) {
			for (int ch = 0; ch < channels; ch++)
				for (int i = 0; i < nb_last; i++)normalized_samples[ch][i] = seqs[ch][i];
		}
		else index2 = nb_last;
		for (int i = nb_last; i < length; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				normalized_samples[ch][i] = static_cast<double>(tempi[index] >> 8) / 8388607.0;
			}
		for (int i = index2; i < nb_last_next; i++)
			for (int ch = 0; ch < channels; ch++, index++) {
				seqs[ch][i] = static_cast<double>(tempi[index] >> 8) / 8388607.0;
			}
		return 0;
	}

}