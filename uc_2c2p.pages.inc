<?php
/**
 * Generate Payment Form
 * @param $order_id
 */
function uc_2c2p_order($order_id) {
  // Load order
  $order = uc_order_load($order_id);

  // Prepare values
  $data = _uc_2c2p_get_args($order);

  // Get Payment URL
  $url = (variable_get('uc_2c2p_testmode', 'yes') === 'yes') ? 'https://demo2.2c2p.com/2C2PFrontEnd/RedirectV3/payment' : 'https://t.2c2p.com/RedirectV3/payment';

  echo '<strong>' . t('Please wait...') . '</strong>';
  echo '<script type="application/javascript">';
  echo 'window.onload = function() { document.getElementById(\'form_2c2p\').submit(); }';
  echo '</script>';
  echo '<form action="'. $url . '" method="post" id="form_2c2p">';
  foreach ($data as $key => $value) {
    echo '<input type="hidden" name="' . $key . '" value="'. $value .'" />';
  }
  echo '</form>';
  exit();
}

/**
 * Redirect Page after Payment
 */
function uc_2c2p_confirm() {
  if (isset($_POST['hash_value'])) {
    // Process payment data
    // Validate data
    $required_fields = array(
      'version',
      'request_timestamp',
      'merchant_id',
      'order_id',
      'invoice_no',
      'currency',
      'amount',
      'transaction_ref',
      'approval_code',
      'eci',
      'transaction_datetime',
      'payment_channel',
      'payment_status',
      'channel_response_code',
      'channel_response_desc',
      'masked_pan',
      'stored_card_unique_id',
      'paid_channel',
      'paid_agent',
      'user_defined_1',
      'user_defined_2',
      'user_defined_3',
      'user_defined_4',
      'user_defined_5',
      'browser_info'
    );

    // Calculate hash
    $tmp = '';
    foreach ($required_fields as $key) {
      $tmp .= $_POST[$key];
    }

    $secret_key = variable_get('uc_2c2p_merchant_authentication_key', '');
    $hash_value = hash_hmac('sha1', $tmp, $secret_key, FALSE);
    if (mb_strtoupper($hash_value) !== mb_strtoupper($_POST['hash_value'])) {
      // Validation is failed
      watchdog('2C2P error', 'Hash validation is failed', $_POST, WATCHDOG_WARNING);
      exit('Hash validation is failed');
    }

    // Order Id
    $order_id = (int) $_POST['order_id'];

    // Check Transaction is exists
    $transaction = _uc_2c2p_transaction_load($_POST['transaction_ref']);
    if (!$transaction) {
      // Save Transaction
      _uc_2c2p_transaction_add($order_id, $_POST['transaction_ref'], $_POST['payment_status'], $_POST);

      // Update order status
      _uc_2c2p_process_transaction($order_id, $_POST);
    }

    // Check Payment status
    switch ($_POST['payment_status']) {
      case '000':
        // Success payment
        $_SESSION['uc_checkout'][$_SESSION['cart_order']]['do_complete'] = TRUE;
        drupal_goto('cart/checkout/complete');
        break;
      case '001':
        // Waiting customer to pay
        break;
      case '002':
        // Error: Payment rejected
        $message = t('Payment rejected.');
        drupal_set_message($message, 'error');
        drupal_goto('cart/checkout');
        break;
      case '003':
        // Payment canceled by user
        $message = t('Payment canceled by user.');
        drupal_set_message($message, 'error');
        drupal_goto('cart/checkout');
        break;
      case '999':
      default:
        $message = t('Transaction failed.');
        drupal_set_message($message, 'error');
        drupal_goto('cart/checkout');
        break;
    }
  }
}

/**
 * Backend Callback
 */
function uc_2c2p_callback() {
  if (isset($_POST['hash_value'])) {
    // Process payment data
    // Validate data
    $required_fields = array(
      'version',
      'request_timestamp',
      'merchant_id',
      'order_id',
      'invoice_no',
      'currency',
      'amount',
      'transaction_ref',
      'approval_code',
      'eci',
      'transaction_datetime',
      'payment_channel',
      'payment_status',
      'channel_response_code',
      'channel_response_desc',
      'masked_pan',
      'stored_card_unique_id',
      'paid_channel',
      'paid_agent',
      'user_defined_1',
      'user_defined_2',
      'user_defined_3',
      'user_defined_4',
      'user_defined_5',
      'browser_info'
    );

    // Calculate hash
    $tmp = '';
    foreach ($required_fields as $key) {
      $tmp .= $_POST[$key];
    }

    $secret_key = variable_get('uc_2c2p_merchant_authentication_key', '');
    $hash_value = hash_hmac('sha1', $tmp, $secret_key, FALSE);
    if (mb_strtoupper($hash_value) !== mb_strtoupper($_POST['hash_value'])) {
      // Validation is failed
      watchdog('2C2P error', 'Hash validation is failed', $_POST, WATCHDOG_WARNING);
      exit('Hash validation is failed');
    }

    // Order Id
    $order_id = (int) $_POST['order_id'];

    // Check Transaction is exists
    $transaction = _uc_2c2p_transaction_load($_POST['transaction_ref']);
    if (!$transaction) {
      // Save Transaction
      _uc_2c2p_transaction_add($order_id, $_POST['transaction_ref'], $_POST['payment_status'], $_POST);

      // Update order status
      _uc_2c2p_process_transaction($order_id, $_POST);
    }
  }
}

/**
 * Update Order status by Transaction Results
 * @param $order_id
 * @param $data
 */
function _uc_2c2p_process_transaction($order_id, $data) {
  // Load order
  $order = uc_order_load($order_id);

  // Get Payment status
  $payment_status = $data['payment_status'];

  // Check Payment status
  switch ($payment_status) {
    case '000':
      // Success payment
      $message = t('Transaction completed. Transaction Id: @transaction_id', array('@transaction_id' => $data['transaction_ref']));
      uc_order_comment_save($order_id, $order->uid, $message, 'admin');
      uc_payment_enter($order_id, $order->payment_method, $order->order_total, $order->uid, array('transaction_id' => $data['transaction_ref']), $message);
      uc_order_update_status($order_id, uc_order_state_default('completed'));
      break;
    case '001':
      // Waiting customer to pay
      break;
    case '002':
      // Error: Payment rejected
      $message = t('Payment rejected.');
      uc_order_comment_save($order_id, $order->uid, $message, 'admin');
      uc_order_update_status($order_id, uc_order_state_default('canceled'));
      break;
    case '003':
      // Payment canceled by user
      $message = t('Payment canceled by user.');
      uc_order_comment_save($order_id, $order->uid, $message, 'admin');
      uc_order_update_status($order_id, uc_order_state_default('canceled'));
      break;
    case '999':
    default:
      $message = t('Transaction failed.');
      uc_order_comment_save($order_id, $order->uid, $message, 'admin');
      uc_order_update_status($order_id, uc_order_state_default('canceled'));
      break;
  }
}
