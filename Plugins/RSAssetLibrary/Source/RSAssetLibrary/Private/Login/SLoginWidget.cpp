﻿/// Copyright (c) 2024 Hunan MangoXR Tech Co., Ltd. All Rights Reserved.


#include "Login/SLoginWidget.h"
#include "Projectlist/FindAllProjectListApi.h"
#include "Projectlist/FindProjectListApi.h"
#include "IImageWrapper.h"
#include "Login/LoginApi.h"
#include "Login/QrLoginApi.h"
#include "RSAssetLibraryStyle.h"
#include "ProjectContent/SProjectWidget.h"
#include "RSpaceAssetLibApi/Public/Login/GetCaptchaApi.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "ImageWrapper/Public/IImageWrapperModule.h"
#include "SlateExtras.h"
#include "Subsystem/USMSubsystem.h"


#define LOCTEXT_NAMESPACE "MainLoginWidget"

void SLoginWidget::Construct(const FArguments& InArgs)
{

    OnLoginSuccess = InArgs._OnLoginSuccess;
    
    ChildSlot
    [
        CreateLoginUI()
    ];
}

SLoginWidget::~SLoginWidget()
{
}

void SLoginWidget::RestartRolling()
{
    if (bIsQRUI)
    {
        UpdateContentBar(CreateQRCodeLoginUI());
        bIsQRUI = true;
        SwitchLoginButton->SetButtonStyle(&AccountLoginButtonStyle);
    }
    else
    {
        UpdateContentBar(CreateAccountLoginUI());
        bIsQRUI = false;
        SwitchLoginButton->SetButtonStyle(&QRLoginButtonStyle);
    }
}

void SLoginWidget::ResetLoginMsg()
{
    if (LoginStatusMessageText)
    {
        FString StatusMessage = TEXT("");
        LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
        LoginStatusMessageText->SetVisibility(EVisibility::Hidden);
    }
}

FReply SLoginWidget::OnGetCaptchaClicked()
{
    if (PhoneNumberTextBox.IsValid())
    {
        FString MobileNumber = PhoneNumberTextBox->GetText().ToString();
        
        if (!MobileNumber.IsEmpty())
        {
            bIsPhoneNumberEmpty = false;
            
            UGetCaptchaApi* GetCaptchaApi = NewObject<UGetCaptchaApi>();
            if (GetCaptchaApi)
            {
                GetCaptchaApi->SendCaptchaRequest(MobileNumber);
            }

            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (World)
            {
                bIsCountdownActive = true;
                CountdownTime = 60;
                World->GetTimerManager().SetTimer(CountdownTimerHandle, FTimerDelegate::CreateSP(this, &SLoginWidget::UpdateCountdown), 1.0f, true);
            }
        }
        else
        {
            bIsPhoneNumberEmpty = true;
            Invalidate(EInvalidateWidget::Layout);
        }
    }

    return FReply::Handled();
}


FReply SLoginWidget::OnLoginButtonClicked()
{

    float CurrentTime = FPlatformTime::Seconds();
    
    if (CurrentTime - LastClickTime < ClickCooldownTime)
    {
        return FReply::Handled();
    }
    
    LastClickTime = CurrentTime;
    LoginButton->SetEnabled(true);
    FString PhoneNumber = PhoneNumberTextBox->GetText().ToString();
    FString VerificationCode = VerificationCodeTextBox->GetText().ToString();
    bool bIsAgreementChecked = AgreementCheckBox->IsChecked();
    
    bIsPhoneNumberEmpty = PhoneNumber.IsEmpty();
    bIsVerificationCodeEmpty = VerificationCode.IsEmpty();
    bIsAgreementUnchecked = !bIsAgreementChecked;
    
    if (!bIsPhoneNumberEmpty && !bIsVerificationCodeEmpty && bIsAgreementChecked)
    {
        ULoginApi* LoginApi = NewObject<ULoginApi>();
        if (LoginApi)
        {
            FOnLoginResponse OnLoginResponseDelegate;
            OnLoginResponseDelegate.BindLambda([this](const FLoginApiResponse& ApiResponse)
            {
                if (ApiResponse.status == "success")
                {
                    LoginButton->SetEnabled(false);
                    if (LoginStatusMessageText.IsValid())
                    {
                        FString StatusMessage = TEXT("登录成功");
                        LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)));
                        LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
                        LoginStatusMessageText->SetVisibility(EVisibility::Visible);
                    }
                    
                    UFindProjectListApi* FindProjectListApi = NewObject<UFindProjectListApi>();
                    if (FindProjectListApi)
                    {
                        FOnFindProjectListResponse OnFindProjectListResponseDelegate;
                        OnFindProjectListResponseDelegate.BindLambda([this, ApiResponse](UFindProjectListResponseData* ProjectListData)
                        {
                            FUserSessionInfo UserSessionInfo;
                            UserSessionInfo.UserPhoneNumber = ApiResponse.data.userInfo.mobile;
                            UserSessionInfo.UserName = ApiResponse.data.userInfo.nickName;
                            UserSessionInfo.SessionExpireTime = FDateTime::Now().ToUnixTimestamp() + ApiResponse.data.tokenInfo.expire;

                            FUserAndProjectInfo UserAndProjectInfo;
                            UserAndProjectInfo.Uuid = ApiResponse.data.userInfo.uuid;
                            UserAndProjectInfo.Ticket = ApiResponse.data.tokenInfo.ticket;
                            UserAndProjectInfo.ProjectItems = ProjectListData->data;
                            UserAndProjectInfo.UserAvatarUrl = ApiResponse.data.userInfo.avatar;
                            
                            UUSMSubsystem* SessionManager = GEditor->GetEditorSubsystem<UUSMSubsystem>();
                            SessionManager->SaveUserSession(UserSessionInfo);
                            
                            SessionManager->SetCurrentUserAndProjectInfo(UserAndProjectInfo);
                            
                            if (OnLoginSuccess.IsBound())
                            {
                                UpdateContentBar(CreateAccountLoginUI());
                                CountdownTime = 0;
                                OnLoginSuccess.Execute();
                            }
                            LoginButton->SetEnabled(true);
                        });
                        
                        FString Ticket = ApiResponse.data.tokenInfo.ticket;
                        FString Uuid = ApiResponse.data.userInfo.uuid;

                        FindProjectListApi->SendFindProjectListRequest(Ticket, Uuid, OnFindProjectListResponseDelegate);
                    }
                }

                else
                {
                    if (LoginStatusMessageText.IsValid())
                    {
                        FString StatusMessage = TEXT("验证失败，请重新登录");
                        LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
                        LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
                        LoginStatusMessageText->SetVisibility(EVisibility::Visible);
                    }
                    LoginButton->SetEnabled(true);
                }
                
                
            });
            
            LoginApi->SendLoginRequest(PhoneNumber, VerificationCode, OnLoginResponseDelegate);
        }
        return FReply::Handled();
    }

    return FReply::Handled();
}


void SLoginWidget::UpdateCountdown()
{
    if (CountdownTime > 0)
    {
        CountdownTime--;
    }
    else
    {
        bIsCountdownActive = false;
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            World->GetTimerManager().ClearTimer(CountdownTimerHandle);
        }
    }
    
    GetCaptchaButton->Invalidate(EInvalidateWidget::Layout);
}

TSharedRef<SWidget> SLoginWidget::CreateLoginUI()
{

    LoginButtonStyle.SetNormal(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.LoginButton_UnSelected"));
    LoginButtonStyle.SetHovered(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.LoginButton_Hovered"));
    LoginButtonStyle.SetPressed(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.LoginButton_Selected"));

    QRLoginButtonStyle.SetNormal(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.QRLoginIcon"));
    QRLoginButtonStyle.SetHovered(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.QRLoginIcon"));
    QRLoginButtonStyle.SetPressed(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.QRLoginIcon"));

    AccountLoginButtonStyle.SetNormal(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.AccountLoginIcon"));
    AccountLoginButtonStyle.SetHovered(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.AccountLoginIcon"));
    AccountLoginButtonStyle.SetPressed(*FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.AccountLoginIcon"));


        return SNew(SBox)
        .WidthOverride(1280.0f) 
        .HeightOverride(720.0f) 
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SImage)
                .Image(FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.LoginBackground"))
            ]
            
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(400)
                .HeightOverride(540)
                [
                    SNew(SBorder)
                    .BorderImage(FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.LoginBackgroundBlack"))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Right)
                        .Padding(FMargin(0, -2, -2, 0))
                        [
                            SAssignNew(SwitchLoginButton, SButton)
                            .ButtonStyle(&QRLoginButtonStyle)
                            .OnClicked(this, &SLoginWidget::OnSwitchLoginClicked)  // 点击切换到二维码登录
                        ]

                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .Padding(FMargin(0, 0))
                        [
                           SAssignNew(ContentBar, SBox)
                           [
                               CreateAccountLoginUI()
                           ]
                        ]
                        
                        + SVerticalBox::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Top)
                        .AutoHeight()
                        .Padding(FMargin(0, 0, 0, 20))
                        [
                            SAssignNew(LoginStatusMessageText, STextBlock)
                            .ColorAndOpacity(FLinearColor::Red)
                            .Visibility(EVisibility::Hidden)
                        ]
                    ]
                ]
            ]
        ];
}


void SLoginWidget::UpdateContentBar(TSharedRef<SWidget> NewContent)
{
    ContentBar->SetContent(NewContent);
}

void SLoginWidget::HandleQrCodeImageReady(const TArray<uint8>& ImageData)
{
    UTexture2D* QrTexture = CreateTextureFromImage(ImageData);

    if (QrTexture)
    {
        bIsQrCodeValid = true;

        if (QrCodeImage.IsValid())
        {
            QrCodeImage->SetImage(new FSlateImageBrush(QrTexture, FVector2D(240, 240)));
        }
    }
    else
    {
        bIsQrCodeValid = false;
    }
}


UTexture2D* SLoginWidget::CreateTextureFromImage(const TArray<uint8>& ImageData)
{
    UTexture2D* Texture = nullptr;

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
    {
        TArray<uint8> UncompressedRGBA;
        if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
        {
            Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);

            if (Texture && Texture->GetPlatformData())
            {
                void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                if (TextureData)
                {
                    FMemory::Memcpy(TextureData, UncompressedRGBA.GetData(), UncompressedRGBA.Num());
                    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
                    Texture->UpdateResource();
                }
                else
                {
                    // // UE_LOG(LogTemp, Error, TEXT("Failed to lock texture data for writing."));
                }
            }
        }
        else
        {
            // // UE_LOG(LogTemp, Error, TEXT("Failed to uncompress image data."));
        }
    }
    else
    {
        // // UE_LOG(LogTemp, Error, TEXT("Invalid image wrapper or failed to set compressed data."));
    }

    return Texture;

}

TSharedRef<SWidget> SLoginWidget::CreateQRCodeLoginUI()
{
    CreateQrApi = NewObject<UQrLoginApi>();
    if (CreateQrApi)
    {
        CreateQrApi->SetOnQrCodeImageReady(FOnQrCodeImageReady::CreateRaw(this, &SLoginWidget::HandleQrCodeImageReady));

        CreateQrApi->SendCreateQrRequest();
        
        UWorld* World = nullptr;
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.World() != nullptr)
                {
                    World = Context.World();
                    break;
                }
            }
        }
        
        if (World)
        {
            CreateQrApi->StartPollingGetInfo(CreateQrApi->GetQrCodeId(), World);
        }
        CreateQrApi->OnQrCodeStateChanged.AddRaw(this, &SLoginWidget::HandleQrCodeStateChanged); 
    }

    FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);
    FSlateFontInfo AgreementFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    FTextBlockStyle HyperFontStyle = FTextBlockStyle()
    .SetFont(AgreementFont)
    .SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,0.8f)));

    SAssignNew(QrCodeImage, SImage).Image(nullptr);

    return SNew(SBox)
    .WidthOverride(400.f)
    .HeightOverride(540.f)
    [
        SNew(SVerticalBox)
        
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 30, 0, 30))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("扫码登录")))
            .Justification(ETextJustify::Center)
            .Font(TitleFont)
        ]
        
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 40))
        .HAlign(HAlign_Center)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("我已阅读并同意")))
                .Font(AgreementFont)
            ]
            
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SNew(SHyperlink)
                .Text(FText::FromString(TEXT("《R空间用户协议》")))
                .OnNavigate(this, &SLoginWidget::OnUserAgreementClicked)
                .TextStyle(&HyperFontStyle)
            ]
        ]
        
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FMargin(0, 0, 0, 20))
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(240.f)
                .HeightOverride(240.f)
                [
                    SNew(SImage)
                    .Image(FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.NetError"))
                    .Visibility(TAttribute<EVisibility>::Create([this]() -> EVisibility
                    {
                        return bIsQrCodeValid ? EVisibility::Collapsed : EVisibility::Visible;
                    }))
                ]
            ]
            
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(240.f)
                .HeightOverride(240.f)
                .Visibility(TAttribute<EVisibility>::Create([this]() -> EVisibility
                {
                    return bIsQrCodeValid ? EVisibility::Visible : EVisibility::Collapsed;
                }))
                [
                    QrCodeImage.ToSharedRef()
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 20))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Top)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("使用R空间App扫码")))
            .Font(AgreementFont)
        ]
    ];
}


void SLoginWidget::HandleQrCodeStateChanged(const FQrLoginResponseData& QrLoginResponseData)
{
    FString StatusMessage;
    
    FQrUserInfoDetails UserInfo;
    FQrTokenInfo TokenInfo;

    if (!bIsQrCodeValid)
    {
        StatusMessage = TEXT("");
        
        if (LoginStatusMessageText.IsValid())
        {
            LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
            LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f,0.f,0.f,0.f)));
            LoginStatusMessageText->SetVisibility(EVisibility::Hidden);
        }

        return;
    }

    switch (QrLoginResponseData.state)
    {
    case 0:
        StatusMessage = TEXT("二维码未被扫描");
        
        if (LoginStatusMessageText.IsValid())
        {
            LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
            LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f,1.f,1.f,1.f)));
            LoginStatusMessageText->SetVisibility(EVisibility::Visible);
        }
        break;

    case 1:
        StatusMessage = TEXT("请在手机上确认登录");
        
        if (LoginStatusMessageText.IsValid())
        {
            LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
            LoginStatusMessageText->SetVisibility(EVisibility::Visible);
            LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f,1.f,0.f,1.f)));
        }
        break;

    case 3:
        {
            UserInfo = QrLoginResponseData.userInfo.userInfo;
            TokenInfo = QrLoginResponseData.userInfo.tokenInfo;

            StatusMessage = FString::Printf(TEXT("登录成功"));
            
            if (LoginStatusMessageText.IsValid())
            {
                LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
                LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f,1.f,0.f,1.f)));
                LoginStatusMessageText->SetVisibility(EVisibility::Visible);
            }
            
            UFindProjectListApi* FindProjectListApi = NewObject<UFindProjectListApi>();
            if (FindProjectListApi)
            {
                FOnFindProjectListResponse OnFindProjectListResponseDelegate;
                OnFindProjectListResponseDelegate.BindLambda([this, UserInfo, TokenInfo](UFindProjectListResponseData* ProjectListData)
                {
                    if (ProjectListData)
                    {
                        FUserSessionInfo UserSessionInfo;
                        UserSessionInfo.UserPhoneNumber = UserInfo.mobile;
                        UserSessionInfo.UserName = UserInfo.nickname;
                        
                        UserSessionInfo.SessionExpireTime = FDateTime::Now().ToUnixTimestamp() + TokenInfo.expire + TestSessionTimeInSeconds;
                        
                        UUSMSubsystem* SessionManager = GEditor->GetEditorSubsystem<UUSMSubsystem>();
                        SessionManager->SaveUserSession(UserSessionInfo);
                        
                        FUserAndProjectInfo UserAndProjectInfo;
                        UserAndProjectInfo.Uuid = UserInfo.uuid;
                        UserAndProjectInfo.Ticket = TokenInfo.ticket;
                        UserAndProjectInfo.ProjectItems = ProjectListData->data;
                        UserAndProjectInfo.UserAvatarUrl = UserInfo.avatar;

                        SessionManager->SetCurrentUserAndProjectInfo(UserAndProjectInfo);

                        if (OnLoginSuccess.IsBound())
                        {
                            UpdateContentBar(CreateQRCodeLoginUI());
                            if (CreateQrApi.Get())
                            {
                                CreateQrApi->SetExternalState(7);
                            }
                            OnLoginSuccess.Execute();
                        }
                    }
                });

                FindProjectListApi->SendFindProjectListRequest(TokenInfo.ticket, UserInfo.uuid, OnFindProjectListResponseDelegate);
            }
            break;
        }

    case 4:
        StatusMessage = TEXT("用户拒绝登录");
        
        if (LoginStatusMessageText.IsValid())
        {
            LoginStatusMessageText->SetText(FText::FromString(StatusMessage));
            LoginStatusMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f,0.f,0.f,1.f)));
            LoginStatusMessageText->SetVisibility(EVisibility::Visible);
        }
        
        break;

    default:
        StatusMessage = TEXT("未知二维码状态");
        break;
    }


    if (QrCodeStatusTextBlock.IsValid())
    {
        QrCodeStatusTextBlock->SetText(FText::FromString(StatusMessage));
    }
    
    UFindAllProjectListApi* FindAllProjectListApi = NewObject<UFindAllProjectListApi>();
    if (FindAllProjectListApi)
    {
        FOnFindAllProjectListResponse OnFindAllProjectListResponseDelegate;
        OnFindAllProjectListResponseDelegate.BindLambda([this](UFindAllProjectListResponseData* AllProjectListData)
        {
            if (AllProjectListData)
            {
                FString Status = AllProjectListData->status;
                FString Code = AllProjectListData->code;
                FString Message = AllProjectListData->message;

                for (const FAllProjectItem& AllProjectItem : AllProjectListData->data)
                {
                    FString projectNo = AllProjectItem.projectNo;
                    FString dramaNo = AllProjectItem.dramaNo;
                    FString projectName = AllProjectItem.projectName; 
                    FString projectImg = AllProjectItem.projectImg;

                }
            }
        });
        FString Ticket = TokenInfo.ticket;
        FString Uuid = UserInfo.uuid;
        FindAllProjectListApi->SendFindAllProjectListRequest(Ticket, Uuid, OnFindAllProjectListResponseDelegate);
    }


    UFindProjectListApi* FindProjectListApi = NewObject<UFindProjectListApi>();
    if (FindProjectListApi)
    {
        FOnFindProjectListResponse OnFindProjectListResponseDelegate;
        OnFindProjectListResponseDelegate.BindLambda([this](UFindProjectListResponseData* ProjectListData)
        {
            if (ProjectListData)
            {
                FString Status = ProjectListData->status;
                FString Code = ProjectListData->code;
                FString Message = ProjectListData->message;

                for (const FProjectItem& ProjectItem : ProjectListData->data.items)
                {
                    FString ProjectName = ProjectItem.projectName;
                    FString ProjectNo = ProjectItem.projectNo;
                    FString ProjectSvn = ProjectItem.projectSvn;
                    FString CreateTime = ProjectItem.createTime;
                    FString UpdateTime = ProjectItem.updateTime;
                }
            }
        });
        FString Ticket = TokenInfo.ticket;
        FString Uuid = UserInfo.uuid;
        FindProjectListApi->SendFindProjectListRequest(Ticket, Uuid, OnFindProjectListResponseDelegate);
    }
}


TSharedRef<SWidget> SLoginWidget::CreateAccountLoginUI()
{
    FSlateFontInfo DefaultFont = FCoreStyle::GetDefaultFontStyle("Regular", 20);
    FSlateFontInfo ShowMsgFont = FCoreStyle::GetDefaultFontStyle("Regular",8);
    FSlateFontInfo CodeFont = FCoreStyle::GetDefaultFontStyle("Regular",7.8);
    
    FTextBlockStyle CustomTextStyle = FTextBlockStyle()
    .SetFont(ShowMsgFont)
    .SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,0.8f)));

    FTextBlockStyle CodeTextStyle = FTextBlockStyle()
    .SetFont(CodeFont)
    .SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,0.8f)));

    FTextBlockStyle HyperFontStyle = FTextBlockStyle()
    .SetFont(ShowMsgFont)
    .SetColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,0.8f)));
    return SNew(SVerticalBox)
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(0, 60, 0, 20))
    [
        SNew(SBox)
        .HeightOverride(80)
        .WidthOverride(320)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("欢迎回来，M-Creator")))
                .Justification(ETextJustify::Center)
                .Font(DefaultFont)
            ]
        ]
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 10, 65, 5))
    [
        SNew(SBox)
        .HeightOverride(40.0f)
        [
            SNew(SBorder)
            .BorderImage(FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.Phone"))
            .BorderBackgroundColor(FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f)))
            .Padding(FMargin(0))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(PhoneNumberTextBox, SEditableTextBox)
                    .Style(&FEditableTextBoxStyle::GetDefault())
                    .HintText(FText::FromString(TEXT("请输入手机号")))
                    .Padding(FMargin(40, 0, 10, 0))
                    .ForegroundColor(FSlateColor(FLinearColor::Gray))
                    .BackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
                ]
            ]
        ]
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 0, 65, 5))
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("手机号不能为空")))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",8))
        .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)))
        .Visibility_Lambda([this]() { return bIsPhoneNumberEmpty ? EVisibility::Visible : EVisibility::Hidden; })
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 0, 65, 5))
    [
        SNew(SBox)
        .HeightOverride(40.0f)
        [
            SNew(SBorder)
            .BorderImage(FRSAssetLibraryStyle::Get().GetBrush("RSAssetLibrary.Msg"))
            .BorderBackgroundColor(FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f)))
            .Padding(FMargin(0))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                [
                    SAssignNew(VerificationCodeTextBox, SEditableTextBox)
                    .Style(&FEditableTextBoxStyle::GetDefault())
                    .HintText(FText::FromString(TEXT("请输入验证码")))
                    .Padding(FMargin(40, 0, 10, 0))
                    .ForegroundColor(FSlateColor(FLinearColor::Gray))
                    .BackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(10, 0, 0, 0))
                [
                    SNew(SBox)
                    .WidthOverride(70.0f)
                    [
                        SAssignNew(GetCaptchaButton, SButton)
                        .ContentPadding(FMargin(0, 0))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .Text(FText::FromString(TEXT("获取验证码")))
                        .TextStyle(&CodeTextStyle)
                        .ForegroundColor(FSlateColor(FLinearColor::Gray))
                        .ButtonColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.0f)))
                        .Text_Lambda([this]() -> FText {
                           return bIsCountdownActive ? FText::FromString(FString::Printf(TEXT(" %ds"), CountdownTime)) : FText::FromString(TEXT("获取验证码"));})
                        .IsEnabled_Lambda([this]() -> bool {
                           return !bIsCountdownActive;
                       })
                       .OnClicked(this, &SLoginWidget::OnGetCaptchaClicked)
                    ]
                ]
            ]
        ]
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 0, 65, 5))
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("验证码不能为空")))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",8))
        .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)))
        .Visibility_Lambda([this]() { return bIsVerificationCodeEmpty ? EVisibility::Visible : EVisibility::Hidden; })
    ]

    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 0, 65, 5))
    [
            SAssignNew(AgreementCheckBox, SCheckBox)
        .IsChecked(ECheckBoxState::Unchecked)
        .Content()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("我已阅读并同意")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.8f)))
            ]
            
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SHyperlink)
                .Text(FText::FromString(TEXT("《R空间用户协议》")))
                .OnNavigate(this, &SLoginWidget::OnUserAgreementClicked)
                .TextStyle(&HyperFontStyle)
            ]
        ]
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 0, 65, 10))
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("请同意用户协议")))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",8))
        .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)))
        .Visibility_Lambda([this]() { return bIsAgreementUnchecked ? EVisibility::Visible : EVisibility::Hidden; })
    ]
    
    + SVerticalBox::Slot()
    .AutoHeight()
    .Padding(FMargin(65, 10, 65, 0))
    [
        SNew(SBox) 
        .HeightOverride(40.0f)
        [
            SAssignNew(LoginButton, SButton)
            .ButtonStyle(&LoginButtonStyle)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnClicked(this, &SLoginWidget::OnLoginButtonClicked)
            
        ]
    ];
    
}

FReply SLoginWidget::OnSwitchLoginClicked()
{
    if (!bIsQRUI)
    {
        UpdateContentBar(CreateQRCodeLoginUI());
        bIsQRUI = true;
        SwitchLoginButton->SetButtonStyle(&AccountLoginButtonStyle);
        return FReply::Handled();
    }
    else
    {
        UpdateContentBar(CreateAccountLoginUI());
        bIsQRUI = false;
        SwitchLoginButton->SetButtonStyle(&QRLoginButtonStyle);
        CreateQrApi->SetExternalState(6);
        LoginStatusMessageText->SetVisibility(EVisibility::Hidden);
        return FReply::Handled();
    }
}

void SLoginWidget::OnUserAgreementClicked()
{
    FString URL = TEXT("https://api.meta.mg.xyz/spaceapi/am/user/user-agreement?type=1&appId=1");
    FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
}


#undef LOCTEXT_NAMESPACE
