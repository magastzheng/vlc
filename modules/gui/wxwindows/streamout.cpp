/*****************************************************************************
 * streamout.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: streamout.cpp,v 1.21 2003/07/07 07:14:56 adn Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>

#include <vlc/intf.h>

#if defined MODULE_NAME_IS_skins
#   include "../skins/src/skin_common.h"
#endif

#include "wxwindows.h"

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

enum
{
    PLAY_ACCESS_OUT = 0,
    FILE_ACCESS_OUT,
    HTTP_ACCESS_OUT,
    UDP_ACCESS_OUT,
    RTP_ACCESS_OUT,
    ACCESS_OUT_NUM
};

enum
{
    TS_ENCAPSULATION = 0,
    PS_ENCAPSULATION,
    AVI_ENCAPSULATION,
    OGG_ENCAPSULATION,
    ENCAPS_NUM,
    MP4_ENCAPSULATION
};

enum
{
    SAP_MISC = 0
};


/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event = wxID_HIGHEST,
    MRL_Event,

    FileBrowse_Event,
    FileName_Event,

    AccessType1_Event, AccessType2_Event, AccessType3_Event,
    AccessType4_Event, AccessType5_Event,
    NetPort1_Event, NetPort2_Event, NetPort3_Event,
    NetAddr1_Event, NetAddr2_Event, NetAddr3_Event,

    EncapsulationRadio1_Event, EncapsulationRadio2_Event,
    EncapsulationRadio3_Event, EncapsulationRadio4_Event,
    EncapsulationRadio5_Event,

    VideoTranscEnable_Event, VideoTranscCodec_Event, VideoTranscBitrate_Event,
    AudioTranscEnable_Event, AudioTranscCodec_Event, AudioTranscBitrate_Event,

    SAPMisc_Event, SAPAddr_Event
};

BEGIN_EVENT_TABLE(SoutDialog, wxDialog)
    /* Button events */
    EVT_BUTTON(wxID_OK, SoutDialog::OnOk)
    EVT_BUTTON(wxID_CANCEL, SoutDialog::OnCancel)

    /* Events generated by the access output panel */
    EVT_CHECKBOX(AccessType1_Event, SoutDialog::OnAccessTypeChange)
    EVT_CHECKBOX(AccessType2_Event, SoutDialog::OnAccessTypeChange)
    EVT_CHECKBOX(AccessType3_Event, SoutDialog::OnAccessTypeChange)
    EVT_CHECKBOX(AccessType4_Event, SoutDialog::OnAccessTypeChange)
    EVT_CHECKBOX(AccessType5_Event, SoutDialog::OnAccessTypeChange)
    EVT_TEXT(FileName_Event, SoutDialog::OnFileChange)
    EVT_BUTTON(FileBrowse_Event, SoutDialog::OnFileBrowse)

    EVT_TEXT(NetPort1_Event, SoutDialog::OnNetChange)
    EVT_TEXT(NetAddr1_Event, SoutDialog::OnNetChange)
    EVT_TEXT(NetPort2_Event, SoutDialog::OnNetChange)
    EVT_TEXT(NetAddr2_Event, SoutDialog::OnNetChange)
    EVT_TEXT(NetPort3_Event, SoutDialog::OnNetChange)
    EVT_TEXT(NetAddr3_Event, SoutDialog::OnNetChange)
 
    /* Events generated by the encapsulation panel */
    EVT_RADIOBUTTON(EncapsulationRadio1_Event,
                    SoutDialog::OnEncapsulationChange)
    EVT_RADIOBUTTON(EncapsulationRadio2_Event,
                    SoutDialog::OnEncapsulationChange)
    EVT_RADIOBUTTON(EncapsulationRadio3_Event,
                    SoutDialog::OnEncapsulationChange)
    EVT_RADIOBUTTON(EncapsulationRadio4_Event,
                    SoutDialog::OnEncapsulationChange)
    EVT_RADIOBUTTON(EncapsulationRadio5_Event,
                    SoutDialog::OnEncapsulationChange)

    /* Events generated by the transcoding panel */
    EVT_CHECKBOX(VideoTranscEnable_Event, SoutDialog::OnTranscodingEnable)
    EVT_CHECKBOX(AudioTranscEnable_Event, SoutDialog::OnTranscodingEnable)
    EVT_COMBOBOX(VideoTranscCodec_Event, SoutDialog::OnTranscodingChange)
    EVT_TEXT(VideoTranscCodec_Event, SoutDialog::OnTranscodingChange)
    EVT_COMBOBOX(AudioTranscCodec_Event, SoutDialog::OnTranscodingChange)
    EVT_TEXT(AudioTranscCodec_Event, SoutDialog::OnTranscodingChange)
    EVT_COMBOBOX(VideoTranscBitrate_Event, SoutDialog::OnTranscodingChange)
    EVT_TEXT(VideoTranscBitrate_Event, SoutDialog::OnTranscodingChange)
    EVT_COMBOBOX(AudioTranscBitrate_Event, SoutDialog::OnTranscodingChange)
    EVT_TEXT(AudioTranscBitrate_Event, SoutDialog::OnTranscodingChange)

    /* Events generated by the misc panel */
    EVT_CHECKBOX(SAPMisc_Event, SoutDialog::OnSAPMiscChange)
    EVT_TEXT(SAPAddr_Event, SoutDialog::OnSAPAddrChange)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
SoutDialog::SoutDialog( intf_thread_t *_p_intf, wxWindow* _p_parent ):
    wxDialog( _p_parent, -1, wxU(_("Stream output")),
             wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    /* Create MRL combobox */
    wxBoxSizer *mrl_sizer_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *mrl_box = new wxStaticBox( panel, -1,
                               wxU(_("Stream output MRL")) );
    wxStaticBoxSizer *mrl_sizer = new wxStaticBoxSizer( mrl_box,
                                                        wxHORIZONTAL );
    wxStaticText *mrl_label = new wxStaticText( panel, -1,
                                                wxU(_("Destination Target:")));
    mrl_combo = new wxComboBox( panel, MRL_Event, wxT(""),
                                wxPoint(20,25), wxSize(120, -1), 0, NULL );
    mrl_combo->SetToolTip( wxU(_("You can use this field directly by typing "
        "the full MRL you want to open.\n""Alternatively, the field will be "
        "filled automatically when you use the controls below")) );

    mrl_sizer->Add( mrl_label, 0, wxALL | wxALIGN_CENTER, 5 );
    mrl_sizer->Add( mrl_combo, 1, wxALL | wxALIGN_CENTER, 5 );
    mrl_sizer_sizer->Add( mrl_sizer, 1, wxEXPAND | wxALL, 5 );

    /* Create the output encapsulation panel */
    wxPanel *encapsulation_panel = EncapsulationPanel( panel );

    /* Create the access output panel */
    wxPanel *access_panel = AccessPanel( panel );

    /* Create the transcoding panel */
    wxPanel *transcoding_panel = TranscodingPanel( panel );

    /* Create the Misc panel */
    wxPanel *misc_panel = MiscPanel( panel );
    
    /* Separation */
    wxStaticLine *static_line = new wxStaticLine( panel, wxID_OK );

    /* Create the buttons */
    wxButton *ok_button = new wxButton( panel, wxID_OK, wxU(_("OK")) );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL,
                                            wxU(_("Cancel")) );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( mrl_sizer_sizer, 0, wxEXPAND, 5 );
    panel_sizer->Add( access_panel, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( encapsulation_panel, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( transcoding_panel, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( misc_panel, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( static_line, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxALIGN_LEFT | wxALIGN_BOTTOM |
                      wxALL, 5 );
    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

SoutDialog::~SoutDialog()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void SoutDialog::UpdateMRL()
{
    /* Let's start with the transcode options */
    wxString transcode;
    if( video_transc_checkbox->IsChecked() ||
        audio_transc_checkbox->IsChecked() )
    {
        transcode = wxT("transcode{");
        if( video_transc_checkbox->IsChecked() )
        {
            transcode += wxT("vcodec=") + video_codec_combo->GetValue();
            transcode += wxT(",vb=") + video_bitrate_combo->GetValue();
            if( audio_transc_checkbox->IsChecked() ) transcode += wxT(",");
        }
        if( audio_transc_checkbox->IsChecked() )
        {
            transcode += wxT("acodec=") + audio_codec_combo->GetValue();
            transcode += wxT(",ab=") + audio_bitrate_combo->GetValue();
        }
        transcode += wxT("}");
    }

    /* Encapsulation */
    wxString encapsulation;
    switch( i_encapsulation_type )
    {
    case PS_ENCAPSULATION:
        encapsulation = wxT("ps");
        break;
    case AVI_ENCAPSULATION:
        encapsulation = wxT("avi");
        break;
    case OGG_ENCAPSULATION:
        encapsulation = wxT("ogg");
        break;
    case MP4_ENCAPSULATION:
        encapsulation = wxT("mp4");
        break;
    case TS_ENCAPSULATION:
    default:
        encapsulation = wxT("ts");
        break;
    }

    /* Now continue with the duplicate option */
    wxString dup_opts;
    if( access_checkboxes[PLAY_ACCESS_OUT]->IsChecked() )
    {
        dup_opts += wxT("dst=display");
    }
    if( access_checkboxes[FILE_ACCESS_OUT]->IsChecked() )
    {
        if( !dup_opts.IsEmpty() ) dup_opts += wxT(",");
        dup_opts += wxT("dst=std{access=file,mux=");
        dup_opts += encapsulation + wxT(",url=\"");
        dup_opts += file_combo->GetValue() + wxT("\"}");
    }
    if( access_checkboxes[HTTP_ACCESS_OUT]->IsChecked() )
    {
        if( !dup_opts.IsEmpty() ) dup_opts += wxT(",");
        dup_opts += wxT("dst=std{access=http,mux=");
        dup_opts += encapsulation + wxT(",url=");
        dup_opts += net_addrs[HTTP_ACCESS_OUT]->GetLineText(0);
        dup_opts += wxString::Format( wxT(":%d"),
                                      net_ports[HTTP_ACCESS_OUT]->GetValue() );
        dup_opts += wxT("}");
    }
    if( access_checkboxes[UDP_ACCESS_OUT]->IsChecked() )
    {
        if( !dup_opts.IsEmpty() ) dup_opts += wxT(",");
        dup_opts += wxT("dst=std{access=udp,mux=");
        dup_opts += encapsulation + wxT(",url=");
        dup_opts += net_addrs[UDP_ACCESS_OUT]->GetLineText(0);
        dup_opts += wxString::Format( wxT(":%d"),
                                      net_ports[UDP_ACCESS_OUT]->GetValue() );

        /* SAP only if UDP */
        if( sap_checkbox->IsChecked() )
        {
            dup_opts += wxT(",sap=\"");
            dup_opts += sap_addr->GetLineText(0);
	    dup_opts += wxT("\"");
        }

        dup_opts += wxT("}");
    }
    if( access_checkboxes[RTP_ACCESS_OUT]->IsChecked() )
    {
        if( !dup_opts.IsEmpty() ) dup_opts += wxT(",");
        dup_opts += wxT("dst=std{access=rtp,mux=");
        dup_opts += encapsulation + wxT(",url=");
        dup_opts += net_addrs[RTP_ACCESS_OUT]->GetLineText(0);
        dup_opts += wxString::Format( wxT(":%d"),
                                      net_ports[RTP_ACCESS_OUT]->GetValue() );
        dup_opts += wxT("}");
    }

    wxString duplicate;
    if( !dup_opts.IsEmpty() )
    {
        if( !transcode.IsEmpty() ) duplicate = wxT(":");
        duplicate += wxT("duplicate{") + dup_opts + wxT("}");
    }

    if( !transcode.IsEmpty() || !duplicate.IsEmpty() )
        mrl_combo->SetValue( wxT("#") + transcode + duplicate );
    else
        mrl_combo->SetValue( wxT("") );
}

wxPanel *SoutDialog::AccessPanel( wxWindow* parent )
{
    int i;
    wxPanel *panel = new wxPanel( parent, -1, wxDefaultPosition,
                                  wxSize(200, 200) );

    wxFlexGridSizer *sizer = new wxFlexGridSizer( 2, 4, 20 );
    wxStaticBox *panel_box = new wxStaticBox( panel, -1,
                                              wxU(_("Output Methods")) );
    wxStaticBoxSizer *panel_sizer = new wxStaticBoxSizer( panel_box,
                                                          wxVERTICAL );

    static const wxString access_output_array[] =
    {
        wxU(_("Play locally")),
        wxU(_("File")),
        wxU(_("HTTP")),
        wxU(_("UDP")),
        wxU(_("RTP"))
    };

    for( i=0; i < ACCESS_OUT_NUM; i++ )
    {
        access_checkboxes[i] = new wxCheckBox( panel, AccessType1_Event + i,
                                               access_output_array[i] );
        access_subpanels[i] = new wxPanel( panel, -1 );
    }

    /* Play locally row */
    wxFlexGridSizer *subpanel_sizer;
    wxStaticText *label;
    label = new wxStaticText( access_subpanels[0], -1, wxT("") );
    subpanel_sizer = new wxFlexGridSizer( 1, 1, 20 );
    subpanel_sizer->Add( label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    access_subpanels[0]->SetSizerAndFit( subpanel_sizer );

    /* File row */
    subpanel_sizer = new wxFlexGridSizer( 3, 1, 20 );
    label = new wxStaticText( access_subpanels[1], -1, wxU(_("Filename")) );
    file_combo = new wxComboBox( access_subpanels[1], FileName_Event, wxT(""),
                                 wxPoint(20,25), wxSize(200, -1), 0, NULL );
    wxButton *browse_button = new wxButton( access_subpanels[1],
                                  FileBrowse_Event, wxU(_("Browse...")) );
    subpanel_sizer->Add( label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    subpanel_sizer->Add( file_combo, 1,
                         wxEXPAND | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    subpanel_sizer->Add( browse_button, 0,
                         wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL );

    access_subpanels[1]->SetSizerAndFit( subpanel_sizer );

    /* Net rows */
    for( i = HTTP_ACCESS_OUT; i < ACCESS_OUT_NUM; i++ )
    {
        subpanel_sizer = new wxFlexGridSizer( 4, 1, 20 );
        label = new wxStaticText( access_subpanels[i], -1, wxU(_("Address")) );
        net_addrs[i] = new wxTextCtrl( access_subpanels[i],
                                   NetAddr1_Event + i - HTTP_ACCESS_OUT,
                                   wxT(""), wxDefaultPosition,
                                   wxSize( 200, -1 ), wxTE_PROCESS_ENTER);
        subpanel_sizer->Add( label, 0,
                             wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
        subpanel_sizer->Add( net_addrs[i], 1, wxEXPAND |
                             wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL );

        int val = config_GetInt( p_intf, "server-port" );
        label = new wxStaticText( access_subpanels[i], -1, wxU(_("Port")) );
        net_ports[i] = new wxSpinCtrl( access_subpanels[i],
                                   NetPort1_Event + i - HTTP_ACCESS_OUT,
                                   wxString::Format(wxT("%d"), val),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   0, 16000, val );

        subpanel_sizer->Add( label, 0,
                             wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
        subpanel_sizer->Add( net_ports[i], 0,
                             wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL );

        access_subpanels[i]->SetSizerAndFit( subpanel_sizer );
    }


    /* Stuff everything into the main panel */
    for( i=1; i < ACCESS_OUT_NUM; i++ )
    {
        sizer->Add( access_checkboxes[i], 0,
                    wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL  | wxALL, 5 );
        sizer->Add( access_subpanels[i], 1, wxEXPAND | wxALIGN_CENTER_VERTICAL
                    | wxALIGN_LEFT  | wxALL, 5 );
    }

    panel_sizer->Add( access_checkboxes[0], 0,
                      wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL  | wxALL, 5 );
    panel_sizer->Add( sizer, 1, wxEXPAND | wxTOP, 3 );

    panel->SetSizerAndFit( panel_sizer );

    /* Update access type panel */
    for( i=1; i < ACCESS_OUT_NUM; i++ )
    {
        access_subpanels[i]->Disable();
    }

    return panel;
}

wxPanel *SoutDialog::MiscPanel( wxWindow* parent )
{
    wxPanel *panel = new wxPanel( parent, -1, wxDefaultPosition,
                                  wxSize(200, 200) );

    // wxFlexGridSizer *sizer = new wxFlexGridSizer( 2, 4, 20 );
    wxStaticBox *panel_box = new wxStaticBox( panel, -1,
                                   wxU(_("Miscellaneous Options")) );
    wxStaticBoxSizer *panel_sizer = new wxStaticBoxSizer( panel_box,
                                                          wxVERTICAL );

    /* SAP Row */
    wxStaticText *label;
    wxFlexGridSizer *subpanel_sizer;

    misc_subpanels[SAP_MISC] = new wxPanel( panel, -1 );
    subpanel_sizer = new wxFlexGridSizer( 4, 2, 20 );

    sap_checkbox = new wxCheckBox( misc_subpanels[SAP_MISC], SAPMisc_Event,
                                   wxU(_("SAP Announce")) );
    label = new wxStaticText( misc_subpanels[SAP_MISC], -1,
                              wxU(_("Channel Name ")) );
    sap_addr = new wxTextCtrl( misc_subpanels[SAP_MISC], SAPAddr_Event,
                               wxT(""), wxDefaultPosition,
                               wxSize( 200, -1 ), wxTE_PROCESS_ENTER);

    subpanel_sizer->Add( sap_checkbox, 0,
                         wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    subpanel_sizer->Add( label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    subpanel_sizer->Add( sap_addr, 1, wxEXPAND |
                         wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL );

    misc_subpanels[SAP_MISC]->SetSizerAndFit( subpanel_sizer );

    /* Stuff everything into the main panel */
    panel_sizer->Add( misc_subpanels[SAP_MISC], 1,
                      wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    panel->SetSizerAndFit( panel_sizer );

    /* Update misc panel */
    misc_subpanels[SAP_MISC]->Disable();
    sap_addr->Disable();

    return panel;
} 

wxPanel *SoutDialog::EncapsulationPanel( wxWindow* parent )
{
    int i;
    wxPanel *panel = new wxPanel( parent, -1, wxDefaultPosition,
                                  wxSize(200, 200) );

    wxStaticBox *panel_box = new wxStaticBox( panel, -1,
                                              wxU(_("Encapsulation Method")) );
    wxStaticBoxSizer *panel_sizer = new wxStaticBoxSizer( panel_box,
                                                          wxHORIZONTAL );

    static const wxString encapsulation_array[] =
    {
        wxT("MPEG TS"),
        wxT("MPEG PS"),
        wxT("AVI"),
        wxT("Ogg"),
        wxT("MP4/MOV")
    };

    /* Stuff everything into the main panel */
    for( i=0; i < ENCAPS_NUM; i++ )
    {
        encapsulation_radios[i] =
            new wxRadioButton( panel, EncapsulationRadio1_Event + i,
                               encapsulation_array[i] );
        panel_sizer->Add( encapsulation_radios[i], 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 8 );
    }

    panel->SetSizerAndFit( panel_sizer );

    /* Update encapsulation panel */
    encapsulation_radios[TS_ENCAPSULATION]->SetValue(true);
    i_encapsulation_type = TS_ENCAPSULATION;

    return panel;
}

wxPanel *SoutDialog::TranscodingPanel( wxWindow* parent )
{
    wxPanel *panel = new wxPanel( parent, -1, wxDefaultPosition,
                                  wxSize(200, 200) );

    wxStaticBox *panel_box = new wxStaticBox( panel, -1,
                                              wxU(_("Transcoding options")) );
    wxStaticBoxSizer *panel_sizer = new wxStaticBoxSizer( panel_box,
                                                          wxVERTICAL );

    /* Create video transcoding checkox */
    static const wxString vcodecs_array[] =
    {
        wxT("mpgv"),
        wxT("mp4v"),
        wxT("DIV1"),
        wxT("DIV2"),
        wxT("DIV3"),
        wxT("H263"),
        wxT("I263"),
        wxT("WMV1"),
        wxT("WMV2"),
        wxT("MJPG")
    };
    static const wxString vbitrates_array[] =
    {
        wxT("3000"),
        wxT("2000"),
        wxT("1000"),
        wxT("750"),
        wxT("500"),
        wxT("400"),
        wxT("200"),
        wxT("150"),
        wxT("100")
    };

    wxFlexGridSizer *video_sizer = new wxFlexGridSizer( 4, 1, 20 );
    video_transc_checkbox =
        new wxCheckBox( panel, VideoTranscEnable_Event, wxU(_("Video codec")));
    video_codec_combo =
        new wxComboBox( panel, VideoTranscCodec_Event, wxT("mp4v"),
                        wxPoint(20,25), wxDefaultSize, WXSIZEOF(vcodecs_array),
                        vcodecs_array, wxCB_READONLY );
    wxStaticText *bitrate_label =
        new wxStaticText( panel, -1, wxU(_("Bitrate (kb/s)")));
    video_bitrate_combo =
        new wxComboBox( panel, VideoTranscBitrate_Event, wxT("1000"),
                        wxPoint(20,25), wxDefaultSize,
                        WXSIZEOF(vbitrates_array), vbitrates_array );
    video_sizer->Add( video_transc_checkbox, 0,
                      wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    video_sizer->Add( video_codec_combo, 1,
                      wxEXPAND | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    video_sizer->Add( bitrate_label, 0,
                      wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    video_sizer->Add( video_bitrate_combo, 1,
                      wxEXPAND | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    /* Create audio transcoding checkox */
    static const wxString acodecs_array[] =
    {
        wxT("mpga"),
        wxT("mp3"),
        wxT("a52")
    };
    static const wxString abitrates_array[] =
    {
        wxT("512"),
        wxT("256"),
        wxT("192"),
        wxT("128"),
        wxT("96")
    };

    wxFlexGridSizer *audio_sizer = new wxFlexGridSizer( 4, 1, 20 );
    audio_transc_checkbox =
        new wxCheckBox( panel, AudioTranscEnable_Event, wxU(_("Audio codec")));
    audio_codec_combo =
        new wxComboBox( panel, AudioTranscCodec_Event, wxT("mpga"),
                        wxPoint(20,25), wxDefaultSize, WXSIZEOF(acodecs_array),
                        acodecs_array, wxCB_READONLY );
    bitrate_label =
        new wxStaticText( panel, -1, wxU(_("Bitrate (kb/s)")));
    audio_bitrate_combo =
        new wxComboBox( panel, AudioTranscBitrate_Event, wxT("192"),
                        wxPoint(20,25), wxDefaultSize,
                        WXSIZEOF(abitrates_array), abitrates_array );
    audio_sizer->Add( audio_transc_checkbox, 0,
                      wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    audio_sizer->Add( audio_codec_combo, 1,
                      wxEXPAND | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    audio_sizer->Add( bitrate_label, 0,
                      wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );
    audio_sizer->Add( audio_bitrate_combo, 1,
                      wxEXPAND | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL );

    /* Stuff everything into the main panel */
    panel_sizer->Add( video_sizer, 0,
                      wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    panel_sizer->Add( audio_sizer, 0,
                      wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5 );

    panel->SetSizerAndFit( panel_sizer );

    /* Update transcoding panel */
    wxCommandEvent event( 0, VideoTranscEnable_Event );
    event.SetInt( 0 );
    OnTranscodingEnable( event );
    event.SetId( AudioTranscEnable_Event );
    OnTranscodingEnable( event );

    return panel;
}

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void SoutDialog::OnOk( wxCommandEvent& WXUNUSED(event) )
{
    mrl_combo->Append( mrl_combo->GetValue() );
    mrl = mrl_combo->GetValue();
    EndModal( wxID_OK );
    
}

void SoutDialog::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_CANCEL );
}

void SoutDialog::OnMRLChange( wxCommandEvent& event )
{
    //mrl = event.GetString();
}

/*****************************************************************************
 * Access output panel event methods.
 *****************************************************************************/
void SoutDialog::OnAccessTypeChange( wxCommandEvent& event )
{
    int i;
    i_access_type = event.GetId() - AccessType1_Event;

    access_subpanels[i_access_type]->Enable( event.GetInt() );

    switch( i_access_type )
    {
    case UDP_ACCESS_OUT:
        misc_subpanels[SAP_MISC]->Enable( event.GetInt() );

    case RTP_ACCESS_OUT:
        for( i = 1; i < ENCAPS_NUM; i++ )
        {
            encapsulation_radios[i]->Enable( !event.GetInt() );
        }
        if( event.GetInt() )
        {
            encapsulation_radios[TS_ENCAPSULATION]->SetValue(true);
            i_encapsulation_type = TS_ENCAPSULATION;
        }
        break;
    }
    UpdateMRL();
}

/*****************************************************************************
 * SAPMisc panel event methods.
 *****************************************************************************/
void SoutDialog::OnSAPMiscChange( wxCommandEvent& event )
{
    sap_addr->Enable( event.GetInt() );
    UpdateMRL();
}

/*****************************************************************************
 * SAPAddr panel event methods.
 *****************************************************************************/
void SoutDialog::OnSAPAddrChange( wxCommandEvent& WXUNUSED(event) )
{
    UpdateMRL();
}

/*****************************************************************************
 * File access output event methods.
 *****************************************************************************/
void SoutDialog::OnFileChange( wxCommandEvent& WXUNUSED(event) )
{
    UpdateMRL();
}

void SoutDialog::OnFileBrowse( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog( this, wxU(_("Save file")),
                         wxT(""), wxT(""), wxT("*"), wxSAVE );

    if( dialog.ShowModal() == wxID_OK )
    {
        file_combo->SetValue( dialog.GetPath() );
        UpdateMRL();
    }
}

/*****************************************************************************
 * Net access output event methods.
 *****************************************************************************/
void SoutDialog::OnNetChange( wxCommandEvent& WXUNUSED(event) )
{
    UpdateMRL();
}

/*****************************************************************************
 * Encapsulation panel event methods.
 *****************************************************************************/
void SoutDialog::OnEncapsulationChange( wxCommandEvent& event )
{
    i_encapsulation_type = event.GetId() - EncapsulationRadio1_Event;
    UpdateMRL();
}

/*****************************************************************************
 * Transcoding panel event methods.
 *****************************************************************************/
void SoutDialog::OnTranscodingEnable( wxCommandEvent& event )
{
    switch( event.GetId() )
    {
    case VideoTranscEnable_Event:
        video_codec_combo->Enable( event.GetInt() );
        video_bitrate_combo->Enable( event.GetInt() );
        break;
    case AudioTranscEnable_Event:
        audio_codec_combo->Enable( event.GetInt() );
        audio_bitrate_combo->Enable( event.GetInt() );
        break;
    }

    UpdateMRL();
}

void SoutDialog::OnTranscodingChange( wxCommandEvent& event )
{
    UpdateMRL();
}
